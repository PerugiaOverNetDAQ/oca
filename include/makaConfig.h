/*!
  \file makaConfig.h
  \brief Classes for MAKA configuration and start (de)serialization
  \author Mattia Barbanera (mattia.barbanera@infn.it)
*/

#include <vector>

//#pragma pack(push,1)
#pragma pack(1)
//! \brief Elements and methods for the configuration packet
class configPacket {

  protected:
    uint32_t detNum;                  //!< Number of detectors
    uint32_t pathLen;                 //!< Length of the data path string
    std::vector<uint32_t> addrsSize;  //!< Size of each address

  public:
    uint32_t pktLen;                //!< Total lenght of the packet transmitted
    std::vector<uint32_t> ids;      //!< Detector IDs
    std::vector<uint32_t> ports;    //!< Detector ports
    std::vector<std::string> addrs; //!< Detector addresses
    std::string dataPath;           //!< Data Path
    uint32_t omPreScale;            //!< Prescaling factor toward OM
    bool dataToFile;                //!< If true, write to file
    bool dataToOm;                  //!< If true, send data to UDP OM
    uint32_t* msg;                  //!< Packet to be transmitted

    configPacket(std::vector<uint32_t> _ids, std::vector<uint32_t> _ports,\
                  std::vector<std::string> _addrs, std::string _dataPath,\
                   bool _dataToFile = true, bool _dataToOm = true,\
                   uint32_t _omPreScale = 10) {
      ids = _ids;
      ports = _ports;
      addrs = _addrs;
      dataPath = _dataPath;
      omPreScale = _omPreScale;
      dataToFile = _dataToFile;
      dataToOm = _dataToOm;
      msg = nullptr;
      sizeUpdate();
    };
    
    configPacket(){
      pktLen = 0;
      detNum = 0;
      pathLen = 0;
      dataPath.clear();
      omPreScale = 10;
      dataToFile = true;
      dataToOm = true;
      msg = nullptr;
    };

    ~configPacket(){
      dataPath.clear();
      if (msg != nullptr) free(msg);
      msg = nullptr;
    };

    //! \brief Serialize all the struct fields into a uint32_t* buffer
    void ser() {
      sizeUpdate();
      if (msg != nullptr) free(msg);
      msg = (uint32_t*)malloc(pktLen);
      uint32_t* outInt = msg;
      uint32_t jj, mm;
  
      //Serialize uint32_t
      *outInt = pktLen;
      outInt++;
      *outInt = detNum;
      outInt++;
      *outInt = pathLen;
      outInt++;
  
      //Serialize ids
      for (auto id : ids) {
        *outInt = id;
        outInt++;
      }

      //Serialize ports
      for (auto port : ports) {
        *outInt = port;
        outInt++;
      }
  
      //Serialize address sizes
      for (auto addrSize : addrsSize) {
        *outInt = addrSize;
        outInt++;
      }

      //Serialize OM pre-scale
      *outInt = omPreScale;
      outInt++;

      bool* outBool = (bool*)outInt;
      //Serialize file and om enablers
      *outBool = dataToFile;
      outBool++;
      *outBool = dataToOm;
      outBool++;
  
      char* outChar = (char*)outBool;
      //Serialize addresses
      for (jj=0; jj<detNum; jj++) {
        for (mm=0; mm<addrsSize[jj]; mm++){
          *outChar = addrs[jj][mm];
          outChar++;
        }
      }

      //Serialize data path
      for (mm=0; mm<pathLen; mm++){
          *outChar = dataPath[mm];
          outChar++;
        }

    }

    //! \brief Deserialize a uint32_t* buffer into fields of the struct
    void des(uint32_t* _in) {
      uint32_t desTemp;
      uint32_t* inInt = _in;
      uint32_t jj;

      //Clear all vectors
      ids.clear();
      ports.clear();
      addrsSize.clear();
      addrs.clear();
      dataPath.clear();

      //Deserialize uint32_t
      pktLen = *inInt;
      inInt++;
      detNum = *inInt;
      inInt++;
      pathLen = *inInt;
      inInt++;

      for (jj=0; jj<detNum; jj++) {
        //Deserialize ids
        desTemp = *inInt;
        inInt++;
        ids.push_back(desTemp);
      }

      for (jj=0; jj<detNum; jj++) {
        //Deserialize port
        desTemp = *inInt;
        inInt++;
        ports.push_back(desTemp);
      }

      for (jj=0; jj<detNum; jj++) {
        //Deserialize address length
        desTemp = *inInt;
        inInt++;
        addrsSize.push_back(desTemp);
      }

      //Deserialize OM prescale
      omPreScale = *inInt;
      inInt++;

      bool* inBool = (bool*)inInt;
      //Serialize file and om enablers
      dataToFile = *inBool;
      inBool++;
      dataToOm = *inBool;
      inBool++;

      char* inChar = (char*)inBool;
      //Deserialize addresses
      for (jj=0; jj<detNum; jj++) {
        char addrTmp[16];
        bzero(addrTmp, sizeof(addrTmp));
        memcpy(addrTmp, inChar, addrsSize[jj]);
        inChar += addrsSize[jj];
        addrs.push_back(addrTmp);
      }

      //Deserialize path
      char* dataPathTmp = (char*) malloc(pathLen);
      memcpy(dataPathTmp, inChar, pathLen);
      dataPath = dataPathTmp;
      inChar += pathLen;
    }

    //! \brief Print all the class values
    void dump() {
      printf("Configurations packet:\n");
      printf("  Packet length:        %u\n",  pktLen);
      printf("  Number of detectors:  %u\n",  detNum);
      printf("  Path Length:          %u\n",  pathLen);
      printf("  Path:                 %s\n",  dataPath.c_str());
      printf("  Write to file:        %s\n",  dataToFile?"true":"false");
      printf("  Send to OM:           %s\n",  dataToOm?"true":"false");
      printf("  OM Pre-Scale:         %u\n",  omPreScale);
      printf("  Detectors:\n");
      for (size_t ii=0; ii<ports.size(); ii++){
        printf("      %lu-%u:                %s:%u,%u\n", ii, ids[ii],\
                                  addrs[ii].c_str(), ports[ii], addrsSize[ii]);
      }
    }

    //! \brief << operator overload for streams
    friend std::ostream& operator <<(std::ostream& _os, const configPacket & _cp) {
      _os << "Configurations packet:  " << std::endl;
      _os << "  Packet length:        " << _cp.pktLen << std::endl
          << "  Number of detectors:  " << _cp.detNum << std::endl
          << "  Path Length:          " << _cp.pathLen << std::endl
          << "  Path:                 " << _cp.dataPath << std::endl
          << "  Write to file:        " << _cp.dataToFile << std::endl
          << "  Send to OM:           " << _cp.dataToOm << std::endl
          << "  OM Pre-Scale:         " << _cp.omPreScale << std::endl;
      _os << "  Detectors:" << std::endl;
      for (size_t ii=0; ii<_cp.ports.size(); ii++){
        _os <<"      "<<ii<<"-" <<_cp.ids[ii]<<":                "<<
          _cp.addrs[ii]<<":"<<_cp.ports[ii]<<", "<<_cp.addrsSize[ii]<<std::endl;
      }
      return _os;
    }

    //! \brief Update all the size fields
    void sizeUpdate() {
      if ((ports.size() != addrs.size()) | (ports.size() != ids.size())) {
        printf("Error: ID, port, or address sizes mismatch\n");
        exit(1);
      }

      detNum = ports.size();

      addrsSize.clear();
      uint32_t sizeAddrTot = 0;
      for (auto addr : addrs) {
        addrsSize.push_back(addr.length());
        sizeAddrTot += addr.length();
      }

      pathLen = dataPath.length();

      pktLen = sizeof(pktLen) + sizeof(detNum) + sizeof(pathLen)\
                + detNum*(sizeof(ports[0]) + sizeof(addrsSize[0]) + sizeof(ids[0]))\
                + sizeAddrTot + pathLen + sizeof(omPreScale)\
                + sizeof(dataToFile) + sizeof(dataToOm);

    }

};
//#pragma pack(pop)
//}__attribute__((packed));


#pragma pack(1)
//! \brief Elements and methods for the start packet
class startPacket {
  protected:
    uint32_t typeLen;

  public:
    uint32_t pktLen;
    std::string type;
    uint32_t num;
    uint32_t time;
    uint32_t* msg;

    startPacket(std::string _runType, uint32_t _runNum, uint32_t _runTime) {
      type = _runType;
      num  = _runNum;
      time = _runTime;
      msg = nullptr;
      sizeUpdate();
    };
    
    startPacket(){
      type.clear();
      num  = 0;
      time = 0;
      msg = nullptr;
    };

    ~startPacket(){
      type.clear();
      if (msg != nullptr) free(msg);
      msg = nullptr;
    };

    //! \brief Serialize all the struct fields into a void* buffer
    void ser() {
      sizeUpdate();
      if (msg != nullptr) free(msg);
      msg = (uint32_t*)malloc(pktLen);
      uint32_t* outInt = msg;
  
      //Serialize uint32_t
      *outInt = pktLen;
      outInt++;
      *outInt = num;
      outInt++;
      *outInt = time;
      outInt++;
      *outInt = typeLen;
      outInt++;

      char* outChar = (char*)outInt;
      //Serialize data path
      for (uint32_t mm=0; mm<typeLen; mm++){
          *outChar = type[mm];
          outChar++;
        }

    }

    //! \brief Deserialize a void* buffer into fields of the class
    void des(uint32_t* _in) {
      uint32_t* inInt = (uint32_t*)_in;

      //Clear type
      type.clear();

      //Deserialize uint32_t
      pktLen = *inInt;
      inInt++;
      num = *inInt;
      inInt++;
      time = *inInt;
      inInt++;
      typeLen = *inInt;
      inInt++;

      //Deserialize type
      char* inChar = (char*)inInt;
      char* typeTmp = (char*) malloc(typeLen+1);
      bzero(typeTmp, typeLen+1);
      memcpy(typeTmp, inChar, typeLen);
      type = typeTmp;
    }

    //! \brief Print all the class values
    void dump() {
      printf("Start packet:\n");
      printf("  Packet length:  %u\n",  pktLen);
      printf("  Run number:     %u\n",  num);
      printf("  Unix time:      %u\n",  time);
      printf("  Type Length:    %u\n",  typeLen);
      printf("  Type:           %s\n",  type.c_str());
    }

    //! \brief << operator overload for streams
    friend std::ostream& operator <<(std::ostream& _os, const startPacket & _sp) {
      _os << "Start packet:  " << std::endl;
      _os << "  Packet length:  " << _sp.pktLen << std::endl
          << "  Run number:     " << _sp.num << std::endl
          << "  Unix time:      " << _sp.time << std::endl
          << "  Type Length:    " << _sp.typeLen << std::endl
          << "  Type:           " << _sp.type << std::endl;
      return _os;
    }

    //! \brief Update all the size fields
    void sizeUpdate() {
      typeLen = type.length();

      pktLen = sizeof(pktLen) + sizeof(num) + sizeof(time)\
                + sizeof(typeLen) + typeLen;

    }
};