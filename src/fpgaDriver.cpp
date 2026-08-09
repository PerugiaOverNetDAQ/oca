#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include "hwlib.h"
#include <string.h>


#include "hps_0.h"
#include "user_avalon_fifo_regs.h"

#include "utility.h"
#include "fpgaDriver.h"
#include "axiFifo.h"
#include "paperoProtocol.h"

namespace {
//Capacità in word della FIFO usata dai pacchetti di configurazione
constexpr uint32_t kConfigFifoDepth = 1024u;
//Numero massimo di payload word inviate in ogni blocco
constexpr size_t kInjectionChunkWords = 256u;
//Numero di payload word caricate prima del comando di avvio
constexpr size_t kInjectionPreloadWords = 3u * paperoProtocol::kHefPayloadWords;
//Livello massimo usato per lasciare spazio a due blocchi
constexpr uint32_t kInjectionBufferLimit = paperoProtocol::kInjectionFifoDepth - 2u * kInjectionChunkWords;
}


fpgaDriver::fpgaDriver(int verbose){
  kVerbose     = verbose;
  
  printf("Opening /dev/mem...\n");
	int fd;
	if((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
		printf("ERROR: could not open \"/dev/mem\"...\n");
		return;
	}

  printf("Mapping FPGA resources...\n");
	virtualBase = mmap(NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ),
                      MAP_SHARED, fd, HW_REGS_BASE);
	if(virtualBase == MAP_FAILED) {
		printf("ERROR: mmap() failed...\n");
		close(fd);
		return;
	}
  
  //Base address of the RegisterArray address: shifts are in units of bytes
  raAddr = (uint32_t*)((unsigned long)virtualBase + ((unsigned long)(ALT_LWFPGASLVS_OFST + REGADDR_PIO_BASE) & (unsigned long)(HW_REGS_MASK)));
  //Base address of the RegisterArray readback: shifts are in units of bytes
  raCont = (uint32_t*)((unsigned long)virtualBase + ((unsigned long)(ALT_LWFPGASLVS_OFST + REGCONTENT_PIO_BASE) & (unsigned long)(HW_REGS_MASK)));

  //FIXME fetch the GW version from gitlab and not from FPGA
  ReadReg(rGW_VER, &kGwV);

  //Instantiate the three FIFOs
  confFifo = new axiFifo(virtualBase, FIFO_HPS_TO_FPGA_IN_BASE,
                          FIFO_HPS_TO_FPGA_IN_CSR_BASE, 3, 1000, 0);
  hkFifo = new axiFifo(virtualBase, FIFO_FPGA_TO_HPS_OUT_BASE,
                        FIFO_FPGA_TO_HPS_OUT_CSR_BASE, 3, 1000, 0);
  dataFifo = new axiFifo(virtualBase, FAST_FIFO_FPGA_TO_HPS_OUT_BASE,
                          FAST_FIFO_FPGA_TO_HPS_OUT_CSR_BASE,
                          paperoProtocol::kFastFifoAlmostEmpty,
                          paperoProtocol::kFastFifoAlmostFull, 0);

  if (kVerbose > 3) {
    printf("FIFO Status post init:\n");
    confFifo->Status();
    hkFifo->Status();
    dataFifo->Status();
  }

  //Stop triggers (if any) and reset FPGA
  SetMode(0);
  ResetFpga();
};

fpgaDriver::~fpgaDriver(){
  StopInjectionThread();
};

uint8_t fpgaDriver::Parity32(uint32_t dataIn){
  uint8_t parity = 0;
  uint8_t nibbles;
  for (int ii = 0; ii < 4; ii++){
    nibbles = (dataIn >> ii*8) & 0xFF;
    parity = parity | ((Parity8(nibbles))<<ii);
  }
  return parity;
}

uint32_t fpgaDriver::CrcUpdate(uint32_t crc, const void* data, size_t data_len){
  const unsigned char* d = (const unsigned char*)data;
  unsigned int i;
  bool bit;
  unsigned char c;
  while (data_len--) {
    c = d[data_len];
    for (i = 0; i < 8; i++) {
        bit = crc & 0x80000000;
        crc = (crc << 1) | ((c >> (7 - i)) & 0x01);
        if (bit) {
            crc ^= 0x04c11db7;
        }
    }
    crc &= 0xffffffff;
  }
  return crc & 0xffffffff;
}

uint32_t fpgaDriver::CrcFinalize(uint32_t crc){
  unsigned int i;
  bool bit;
  for (i = 0; i < 32; i++) {
    bit = crc & 0x80000000;
    crc <<= 1;
    if (bit) {
        crc ^= 0x04c11db7;
    }
  }
  return crc & 0xffffffff;
}

void fpgaDriver::ReadReg(int regAddr, uint32_t* data){
  //Impedisce che due thread cambino indirizzo durante una lettura
	std::lock_guard<std::mutex> lock(readMutex);
	//Write the address of the register to be read
	*raAddr = regAddr;
	//Read the register content
	*data = *raCont;
  if(kVerbose > 2){
    printf("%s) ReadREG: Register addr: %d - content: %08x\n", __METHOD_NAME__, regAddr, *data);
  }
}

void fpgaDriver::SingleWriteReg(uint32_t regAddr, uint32_t regContent){
  uint32_t singleWrite[2];
  singleWrite[0] = regContent;
  singleWrite[1] = regAddr;
  WriteReg(singleWrite, 2);
}

int fpgaDriver::WriteReg(uint32_t* pktContent, int pktLen){
  //Mantiene unito ogni pacchetto scritto nella FIFO di configurazione
  std::lock_guard<std::mutex> lock(configMutex);
  uint32_t packet[pktLen+6];
  uint8_t parityMsb, parityLsb;
  uint32_t pktCrc;

  //Create the packet header
  pktCrc = CrcInit();
  packet[0] = REG_SOP;
  packet[1] = (uint32_t)pktLen+5;
  packet[2] = kGwV; //@todo fetch the GW version from github and not from FPGA
  pktCrc = CrcUpdate(pktCrc, &packet[2], sizeof(uint32_t));
  packet[3] = REG_HDR1;
  pktCrc = CrcUpdate(pktCrc, &packet[3], sizeof(uint32_t));

  //Create the packet body
  for(int ii=0; ii<pktLen; ii=ii+2){
    parityLsb = Parity32(pktContent[ii]);
    parityMsb = Parity32(pktContent[ii+1]);
    packet[ii+4] = pktContent[ii];
    packet[ii+5] = ((uint32_t)parityMsb<<28) | ((uint32_t)parityLsb<<24) | pktContent[ii+1];
    pktCrc = CrcUpdate(pktCrc, &packet[ii+4], sizeof(uint32_t));
    pktCrc = CrcUpdate(pktCrc, &packet[ii+5], sizeof(uint32_t));
  }

  //Create the packet footer
  packet[pktLen+4] = REG_EOP;
  pktCrc = CrcFinalize(pktCrc);
  packet[pktLen+5] = pktCrc;

  if (kVerbose > 3){
    printf("%s) WriteReg: Packet Content:\n", __METHOD_NAME__);
    for (int jj=0; jj<pktLen+6;jj++){
      printf("%08x\n", packet[jj]);
    }
  }

  //Calcola il numero totale di word inclusi header e footer
  const uint32_t packetWords = static_cast<uint32_t>(pktLen + 6);
  //Rifiuta pacchetti che non possono entrare nella FIFO
  if (packetWords > kConfigFifoDepth) {
    fprintf(stderr, "%s) Configuration packet is too large: %u words\n",
            __METHOD_NAME__, packetWords);
    return 1;
  }

  //Imposta il limite massimo per il tempo di attesa dello spazio libero. MAX 10 secondi
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  //Attende spazio sufficiente per scrivere il pacchetto completo
  //Lo spazio è quello della fifo di configurazione 1024 word da 32 bit = 4096 byte 
  while (confFifo->getUsedw() + packetWords > kConfigFifoDepth) {
    //Overtime, interruzione scrittura
    if (std::chrono::steady_clock::now() >= deadline) {
      fprintf(stderr, "%s) Timeout waiting for configuration FIFO space\n", __METHOD_NAME__);
      return 1;
    }
    usleep(100);
  }
  //Scrive tutte le word tramite accessi memory mapped alla FIFO
  if (confFifo->writeChunck(packet, pktLen+6) < 0) {
    return 1;
  }
  return 0;
}

int fpgaDriver::WaitConfigFifoEmpty(uint32_t timeoutMs) {
  //TImeouttime
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  //Finché la fifo di configurazione non è vuota
  while (!confFifo->getEmpty()) {
    //Errore se overtime
    if (std::chrono::steady_clock::now() >= deadline) {
      fprintf(stderr, "%s) Timeout waiting for configuration FIFO drain\n", __METHOD_NAME__);
      return 1;
    }
    usleep(100);
  }
  return 0;
}

//Passo la prima parola e quante ne devo trasferire
int fpgaDriver::WriteInjectionWords(size_t first, size_t count) {
  //Alloca una coppia valore e indirizzo per ogni payload word
  //Quindi moltiplico il contatore pre 2
  std::vector<uint32_t> writes(2u * count);
  //Costruisce le scritture dirette al registro dati di iniezione
  for (size_t i = 0; i < count; ++i) {
    writes[2u*i] = injectionWords[first+i]; //Parola da scrivere
    writes[2u*i+1u] = rINJECT_DATA; //12. Registro di destinazione 

    //Parola Registro Parola Registro ...
  }
  //Invio il pacchetto con la FIFO di configurazione
  if (WriteReg(writes.data(), static_cast<int>(writes.size())) != 0) {
    return 1;
  }
  //Attende che il config receiver consumi tutte le scritture
  //MAX 5 secondi
  return WaitConfigFifoEmpty(5000);
}

void fpgaDriver::StopInjectionThread() {
  //Comunica la terminazione
  injectionStop = true;
  //Attende la chiusura solo quando il thread esiste
  if (injectionThread.joinable()) {
    injectionThread.join();
  }
}

void fpgaDriver::GetInjectionStatus(uint32_t& status) {
  //Legge flag e livello dal registro pubblicato dalla FPGA
  ReadReg(rINJECT_STATUS, &status); //INJ status è il 28
}

int fpgaDriver::PrepareInjection(const std::vector<uint32_t>& words) {
  StopInjectionThread();
  injectionWords = words; //Copia il payload ricevuto nella memoria HPS del driver
  injectionNext = 0u;     //Riparte dalla prima word del nuovo payload
  injectionStop = false;

  //Azzera la FIFO di iniezione e lo stato della logica FPGA
  SingleWriteReg(rINJECT_CTRL, 1u); //Scrivo nel registro di controllo 13 1 unsigned = bit0 = 1 e quindi RESET

  //Attendo il reset
  if (WaitConfigFifoEmpty(5000) != 0) {
    return 1;
  }

  //Limita il preload a tre eventi (nella FIFO FPGA) o ai dati realmente disponibili
  const size_t preload = std::min(injectionWords.size(), kInjectionPreloadWords);

  //Trasferisce il preload  
  while (injectionNext < preload) {
    //Il minimo serve a caricare almeno 3 eventi completi oppure meno se ho finito gli eventi
    const size_t count = std::min(kInjectionChunkWords, preload - injectionNext);

    //Trasferimento effettivo, se non torna 0 c'è errore
    if (WriteInjectionWords(injectionNext, count) != 0) {
      return 1;
    }
    //Avanzo il count se il trasferimento è andato a buon fine
    injectionNext += count;
  }

  //Segnala la fine del flusso quando tutto il payload è già precaricato
  if (injectionNext == injectionWords.size()) {
    SingleWriteReg(rINJECT_CTRL, 2u); // registro 13 INJECT CTRL valore 2, fine flusso

    //Verifica che il segnale di fine flusso sia arrivato alla FPGA
    if (WaitConfigFifoEmpty(5000) != 0) {
      return 1;
    }
  }

  //Imposta il limite massimo per la verifica del preload
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  uint32_t status = 0u;
  //Legge il livello fino alla presenza di tutte le word precaricate
  do {
    GetInjectionStatus(status); //Leggo il registro 28 dell'FPGA con
    //bit 0       active
    //bit 1       failed
    //bit 2       done
    //bit 3       ready
    //bit 15 a 4  altri campi o riservati
    //bit 31 a 16 numero di word presenti nella FIFO

    //Faccio shift e prendo solo il numero di WORD e lo confronto con quello da precaricare
    //Se ho fatto torno 0
    if ((status >> paperoProtocol::kInjectionUsedWordsShift) >= preload) {
      return 0;
    }

    usleep(1000); //Aspetta 1sec e riverifica
  } while (std::chrono::steady_clock::now() < deadline); //Se timeout allora errore

  //Segnala che la FPGA non ha ricevuto il preload entro il timeout
  fprintf(stderr, "%s) Injection preload did not reach the FPGA\n", __METHOD_NAME__);
  return 1; //Errore
}

void fpgaDriver::FeedInjection() {
  //Finche ci sono dati o è attivo l'invio
  while (!injectionStop.load() && injectionNext < injectionWords.size()) {

    uint32_t status = 0u;
    GetInjectionStatus(status); //Lettura stato corrente

    //Controllo sui flag di errore e completamento
    const bool injectionFailed = (status & paperoProtocol::kInjectionFailedMask) != 0u;
    const bool injectionDone = (status & paperoProtocol::kInjectionDoneMask) != 0u;
    //Ferma quando la FPGA segnala errore o completamento
    if (injectionFailed || injectionDone) {
      break;
    }

    //Estrae il numero di word presenti nella FIFO di iniezione, con il classico shift di 16 per prendere il count
    const uint32_t usedWords = status >> paperoProtocol::kInjectionUsedWordsShift;

    //Attende quando il buffer ha raggiunto il livello massimo previsto
    if (usedWords >= kInjectionBufferLimit) {
      usleep(1000);
      continue;
    }

    //Calcola quante word restano nella memoria HPS
    const size_t remainingWords = injectionWords.size() - injectionNext;
    //Calcola quante word possono essere aggiunte alla FIFO
    const size_t availableWords = static_cast<size_t>(kInjectionBufferLimit - usedWords);

    //Limita il blocco al minimo tra dimensione massima, dati rimasti e spazio libero
    const size_t wordsToWrite = std::min({kInjectionChunkWords, remainingWords, availableWords});

    //Stop se non c'è un blocco valido
    if (wordsToWrite == 0u) {
      break;
    }

    const int writeResult = WriteInjectionWords(injectionNext, wordsToWrite); //Trasferisce il blocco dalla memoria HPS alla FPGA

    //Trasferimento fallito, allora stop
    if (writeResult != 0) {
      break;
    }
    injectionNext += wordsToWrite; //Avanza alla prima word che deve ancora essere trasferita
  }

  const bool allWordsTransferred = injectionNext == injectionWords.size(); //Payload non interamente trasferito
  const bool feederStillActive = !injectionStop.load();  //Chiusura non richiesta

  //Segnalo con INJECT CTRL che non arrivano altre word
  if (feederStillActive && allWordsTransferred) {
    SingleWriteReg(rINJECT_CTRL, 2u);
    WaitConfigFifoEmpty(5000);
  }
}

void fpgaDriver::CancelInjection() {
  
  StopInjectionThread(); //Ferma thread scrittura FPGA
  SingleWriteReg(rINJECT_CTRL, 1u); //RESET
  WaitConfigFifoEmpty(5000); //Attesa appl reset. 5sec
  injectionWords.clear();
  injectionNext = 0u; //Azzeramento posizione prossima word
}

void fpgaDriver::ResetFpga(){
	StopInjectionThread(); //Ferma injection 
	uint32_t data[4096];
	int flushErr=0;
	//Set to high the regArray bits of reset
	SingleWriteReg((uint32_t)rGOTO_STATE, 0x00000003);

	//Flush the FastData Fifo
	flushErr = dataFifo->readChunk(data, 0, true);
	if(kVerbose > 0) printf("%s) Flushed %d words from DATA FIFO\n", __METHOD_NAME__, flushErr);
  dataPacketPending = false;
  dataPacketLength = 0;
  flushErr = hkFifo->readChunk(data, 0, true);
	if(kVerbose > 0) printf("%s) Flushed %d words from HK FIFO\n", __METHOD_NAME__, flushErr);

	//Remove regArray reset
	SingleWriteReg((uint32_t)rGOTO_STATE, 0x00000000);
}

void fpgaDriver::ResetCounters(){
  SingleWriteReg((uint32_t)rGOTO_STATE,
                 paperoProtocol::kCounterResetBit);
  SingleWriteReg((uint32_t)rGOTO_STATE,
                 paperoProtocol::kStopCommand);
}

void fpgaDriver::InitFpga(uint32_t* regsContentIn, uint32_t opLen){
  //Configure the whole regArray (except register rGOTO_STATE)
  WriteReg(regsContentIn, opLen);
  
	//Reset the FPGA
	ResetFpga();
}

void fpgaDriver::SetDelay(uint32_t delayIn){
	uint32_t regContent;
	ReadReg(rMSD_PARAM, &regContent);
	regContent = (regContent & 0xFFFF0000) | (delayIn & 0x0000ffff);
	SingleWriteReg(rMSD_PARAM, regContent);
}

void fpgaDriver::SetMode(uint32_t modeIn){
  SingleWriteReg(rGOTO_STATE, modeIn);
}

void fpgaDriver::StartAcquisition(uint32_t command, uint32_t thresholds){
  //Chiude il feeder collegato al run precedente
  StopInjectionThread();
  if ((command & paperoProtocol::kThresholdBit) != 0u) {
    // PAPERO samples REG11 together with the other fields on the
    // REG0.RUN_REQUEST rising edge. Keep both writes in one ordered packet.
    uint32_t runConfig[4] = {
      thresholds,
      rTHR_PARAM,
      command | paperoProtocol::kRunRequestBit,
      rGOTO_STATE
    };
    WriteReg(runConfig, 4);
  }
  else {
    SingleWriteReg(rGOTO_STATE,
                   command | paperoProtocol::kRunRequestBit);
  }

  //Attende la consegna del comando prima di avviare il feeder
  if ((command & paperoProtocol::kInjectBit) != 0u && WaitConfigFifoEmpty(5000) == 0) {
    //Abilita e avvia il thread che alimenta la FIFO di iniezione
    injectionStop = false;
    injectionThread = std::thread(&fpgaDriver::FeedInjection, this);
  }
}

void fpgaDriver::StopAcquisition(){
  StopInjectionThread();
  SingleWriteReg(rGOTO_STATE, paperoProtocol::kStopCommand);
}

void fpgaDriver::GetEventNumber(uint32_t* extTrigCount, uint32_t* intTrigCount){
	ReadReg(rEXT_TRG_COUNT, extTrigCount);
	ReadReg(rINT_TRG_COUNT, intTrigCount);
}

void fpgaDriver::Calibrate(uint32_t calibIn){
	uint32_t regContent;
	ReadReg(rTRIGBUSY_LOGIC, &regContent);
	regContent = (regContent & 0xFFFFFFFD) | (calibIn & 0x00000002);
	SingleWriteReg(rTRIGBUSY_LOGIC, regContent);
}

void fpgaDriver::intTriggerPeriod(uint32_t periodIn){
	uint32_t regContent;
	ReadReg(rTRIGBUSY_LOGIC, &regContent);
	regContent = (periodIn & 0xFFFFFFF0) | (regContent & 0x0000000F);
	SingleWriteReg(rTRIGBUSY_LOGIC, regContent);
}

void fpgaDriver::selectTrigger(uint32_t intTrigIn){
	uint32_t regContent;
	ReadReg(rTRIGBUSY_LOGIC, &regContent);
	regContent = (regContent & 0xFFFFFFFE) | (intTrigIn & 0x00000001);
	SingleWriteReg(rTRIGBUSY_LOGIC, regContent);
}

void fpgaDriver::configureTestUnit(uint32_t tuCfg){
	uint32_t regContent;
	ReadReg(rUNITS_EN, &regContent);
	regContent = (regContent & 0xFFFFFCFD) | (tuCfg & 0x00000302);
	SingleWriteReg(rUNITS_EN, regContent);
}

void fpgaDriver::setFeClk(uint32_t _feClkParams){
  SingleWriteReg(rFE_CLK_PARAM, _feClkParams);
}

void fpgaDriver::setAdcClk(uint32_t _adcClkParams){
  SingleWriteReg(rADC_CLK_PARAM, _adcClkParams);
}

void fpgaDriver::setIdeTest(uint32_t _ideTest){
  uint32_t regContent;
  ReadReg(rMSD_PARAM, &regContent);
  regContent = (regContent & 0xFE00FFFF) | (_ideTest & 0x01FF0000);
  SingleWriteReg(rMSD_PARAM, regContent);
}

void fpgaDriver::setAdcFast(uint32_t _adcFast){
  uint32_t regContent;
  ReadReg(rMSD_PARAM, &regContent);
  regContent = (regContent & 0x7FFFFFFF) | (_adcFast & 0x80000000);
  SingleWriteReg(rMSD_PARAM, regContent);
}

void fpgaDriver::setBusyLen(uint32_t _busyLen){
  uint32_t regContent;
  ReadReg(rBUSYADC_PARAM, &regContent);
  regContent = (regContent & 0x0000FFFF) | (_busyLen & 0xFFFF0000);
  SingleWriteReg(rBUSYADC_PARAM, regContent);
}

void fpgaDriver::setAdcDelay(uint32_t _adcDelay){
  uint32_t regContent;
  ReadReg(rBUSYADC_PARAM, &regContent);
  regContent = (regContent & 0xFFFF0000) | (_adcDelay & 0x0000FFFF);
  SingleWriteReg(rBUSYADC_PARAM, regContent);
}

void fpgaDriver::biasTranslate(float biasIn, uint32_t &dacOut) {
  // LT3482: Vctrl in the range 0-1.371V corresponding to 0-90V output. If Vctrl > 1.5, Vout = 90V
  // R1 = R2*(Vo/Vctrl - 1), R1 = 1MOhm, R2 = 15kOhm
  // Vo = (R1/R2 -1)*Vctrl = 66.666667*Vctrl
  // Vctrl = Vo/66.667 = 0.015228*Vo
  // 
  // LTC1663: DAC 10-bit resolution, Vref = 2.5V
  // Vout = DAC/(2^Nbits-1)*Vref = (DAC/1023)*2.5V = 0.002444*DAC
  // DAC = Vout/Vref*(2^Nbits-1)
  //
  // Vout = Vctrl
  // DAC = Vctrl/Vref*(2^Nbits-1) = Vctrl*1023/2.5 = 409.2*Vctrl
  // Vctrl = 0.015228 Vout -> DAC = 0.015228*Vout*1023/2.5 = 6.231472*Vout
  //
  // 70V -> 1.066V -> 436 , 0x1B4; 50V -> 0.761V -> 312, 0x138;
  const float kConv = (1023/2.5)*(15e3/(1e6-15e3));
  dacOut = (uint32_t)(biasIn*kConv) & 0x000003FF;
}

void fpgaDriver::biasCtrl(float bias0, float bias1) {
  uint32_t bias0Dac = 0;
  uint32_t bias1Dac = 0;
  biasTranslate(bias0, bias0Dac);
  biasTranslate(bias1, bias1Dac);

  // Write values
  uint32_t regCont = (((bias1Dac&0x000003FF)<<16) | (bias0Dac&0x000003FF));
  SingleWriteReg((uint32_t)rBIAS_PARAM, regCont);
  
  // Assert DAC write request for both Biases
  SingleWriteReg((uint32_t)rBIAS_PARAM, (0x80008000) | regCont);
  
  // Dessert DAC write request
  SingleWriteReg((uint32_t)rBIAS_PARAM, (0x00000000) | regCont);
}

void fpgaDriver::biasCurrTranslate(uint32_t _currADC, float& _currA) {
  //LT3482: Iout:Imon 5:1; Imon converted with R6
  //  Iout=(5/R6)⋅Vmon
  //LTC2312: internal reference: Vmax = 4.096 V. LSB: 1 mV
  //  ADC=Vin/LSB
  //
  //Combined: Iout=(5⋅LSB/R6)⋅ADC
  //  LSB=1 mV; R6=20 k;
  //  Iout[mA] = ADC/4000
  const float kConv = 1/4000;
  _currA = ((_currADC)&0x00000FFF)*kConv*1e-3;
}

void fpgaDriver::biasCurrRead(float& _curr0, float& _curr1, uint8_t& _flags) {
  //rBIAS_CURR_MON reg content
  //     31 - LT1663 Ack 1
  //     30 - LT1663 Err 1
  //     29 - LTC2312 Busy
  //     28 - LTC2312 Done
  //[27:16] - Bias Current 1
  //     15 - LT1663 Ack 0
  //     14 - LT1663 Err 0
  // [11:0] - Bias Current 0;

  uint32_t regContent = 0;
  //Read reg
  ReadReg(rBIAS_CURR_MON, &regContent);
  //Populate flags
  _flags = ((regContent&0xF0000000)>>24)|((regContent&0x0000C000)>>12);
  
  //Convert current and update port
  biasCurrTranslate((regContent&0x00000FFF), _curr0);
  biasCurrTranslate((regContent&0x0FFF0000)>>16, _curr1);
}

int fpgaDriver::getEvent(std::vector<uint32_t>& evt, int* evtLen){
  *evtLen = 0;

  // HEF compressed packets have a variable length.  Consume the two-word
  // header as soon as it is available, then wait for precisely the advertised
  // remainder instead of relying on a RAW-packet almost-empty threshold.
  if (!dataPacketPending) {
    if (dataFifo->getUsedw() < 2u) {
      return 0;
    }

    uint32_t sopWord = 0;
    uint32_t pktLen = 0;
    if (dataFifo->read(&sopWord) < 0 || dataFifo->read(&pktLen) < 0) {
      fprintf(stderr, "Error reading event header\n");
      return -1;
    }
    if (sopWord != DATA_SOP) {
      fprintf(stderr, "First value of event not SoP: %08x\n", sopWord);
      return -1;
    }
    if (pktLen < 12u ||
        pktLen > paperoProtocol::kHefMaxPacketLength) {
      fprintf(stderr, "Invalid HEF packet length: %u\n", pktLen);
      return -2;
    }

    dataPacketPending = true;
    dataPacketLength = pktLen;
  }

  const uint32_t remainingWords = dataPacketLength - 1u;
  if (dataFifo->getUsedw() < remainingWords) {
    if (kVerbose > 4) {
      printf("%s) Waiting for HEF packet: %u/%u words available\n",
             __METHOD_NAME__, dataFifo->getUsedw(), remainingWords);
    }
    *evtLen = 0;
    return 0;
  }

  evt.resize(dataPacketLength + 1u);
  evt[0] = DATA_SOP;
  evt[1] = dataPacketLength;
  const int wordsRead =
      dataFifo->readChunk(&evt[2], remainingWords, false);
  if (wordsRead != static_cast<int>(remainingWords)) {
    fprintf(stderr, "Error in reading event\n");
    return -3;
  }

  if (kVerbose > 4){
    printf("%s) Event:\n", __METHOD_NAME__);
    for(uint32_t i = 0; i < dataPacketLength + 1u; i++){
      printf("%08x\n", evt[i]);
    }
  }

  *evtLen = static_cast<int>(dataPacketLength + 1u);
  dataPacketPending = false;
  dataPacketLength = 0;

  return 0;
}
