# OCA Run Control

## Start and stop commands

```text
startOCA <cal|daq|mix> <0|1|int|ext> <save|nosave> [runnum]
startOCA dump [runnum]

stopOCA
```

### Arguments

| Argument   | Meaning                                                                           |
| ---------- | --------------------------------------------------------------------------------- |
| `cal`      | Always compute a new calibration; no normal events                                |
| `daq`      | Reuse a valid calibration; calibrate only if invalid                              |
| `mix`      | Always compute a new calibration, then acquire normal events                      |
| `dump`     | Output cached calibration tables only                                             |
| `0`, `int` | Internal triggers                                                                 |
| `1`, `ext` | External triggers                                                                 |
| `save`     | Serialize calibration tables into the data stream                                 |
| `nosave`   | Keep calibration tables in PAPERO RAM without serializing them                    |
| `runnum`   | Optional run number; default `21`; valid range `0..65535`; decimal or `0x` format |

Examples:

```sh
./exe/startOCA cal int save 42
./exe/startOCA cal ext nosave 43
./exe/startOCA daq ext save 44
./exe/startOCA daq ext nosave 45
./exe/startOCA mix int save 46
./exe/startOCA mix ext nosave 47
./exe/startOCA dump 48
./exe/stopOCA
```

---

## Run modes

| Run mode | Calibration                                     | Normal events                        | End condition                                       |
| -------- | ----------------------------------------------- | ------------------------------------ | --------------------------------------------------- |
| `cal`    | Always new                                      | No                                   | Waits for `stopOCA` after calibration output drains |
| `daq`    | Reuse valid calibration; compute one if invalid | Yes                                  | Runs until `stopOCA`                                |
| `mix`    | Always new                                      | Yes, after calibration output drains | Runs until `stopOCA`                                |
| `dump`   | Never computes                                  | No                                   | Waits for `stopOCA` after the dump                  |

### Important rules

* `save` controls only calibration-table serialization.
* `save` does not control whether calibration is computed.
* `nosave` still stores the completed calibration in PAPERO CALIB RAM.
* All `cal`, `daq` and `mix` trigger/save combinations are valid.
* The canonical `dump` form has no trigger-source or `save/nosave` argument.
* The current parser also accepts the redundant form `dump <0|int|internal> save [runnum]`; forms selecting an external trigger or `nosave` are rejected.
* Run mode `mix` and `DAQ_MODE=MIX` are two different settings.

### Special cases

| Command                               | Behaviour                                                                          |
| ------------------------------------- | ---------------------------------------------------------------------------------- |
| `cal ... nosave`                      | Computes and stores a new calibration, but outputs no tables                       |
| `daq ... save` with valid calibration | Dumps cached tables without recomputing calibration or consuming detector triggers |
| `dump` with `CAL_VALID=1`             | Outputs the four cached tables                                                     |
| `dump` with `CAL_VALID=0`             | Outputs nothing and waits for `stopOCA`                                            |

---

## Calibration validity

Before `daq`, OCA reads `CAL_VALID` from every configured PAPERO.

| Board state                | OCA action                                        |
| -------------------------- | ------------------------------------------------- |
| All boards valid           | Start DAQ directly                                |
| One or more boards invalid | Force all boards to calibrate together before DAQ |

`CAL_VALID`:

* becomes `1` only after a complete calibration;
* remains set across `stopOCA`;
* remains set across acquisition-counter reset;
* is cleared by a full FPGA/detector reset.

Automatic DAQ-calibration triggers are included in the new run trigger count.

Starting a run resets only the acquisition counters. It does not:

* reset the detector interface;
* clear calibration RAM;
* flush DATA FIFOs;
* flush HK FIFOs.

---

## PAPERO normal-event modes

The three optional trailing fields of each active `config/papero.cfg` line are:

```text
DAQ_MODE LTH HTH
```

| Field      | Meaning                          |
| ---------- | -------------------------------- |
| `DAQ_MODE` | Normal-event encoding            |
| `LTH`      | 16-bit low threshold, ADC32      |
| `HTH`      | 16-bit high threshold, ADC32     |

If omitted, the fields default independently to `DAQ_MODE=0`, `LTH=0x0030` and `HTH=0x0070`.

At every `cal`, `daq` or `mix` start, OCA:

1. writes `LTH` and `HTH`;
2. writes the complete `REG0` command with `THR_VALID=1` after the threshold write;
3. lets PAPERO generate the one-clock threshold-application pulse on the following clock.

`dump` does not write thresholds or assert `THR_VALID`.

### `DAQ_MODE` values

|      Value | Name   | Normal-event payload                                         | Trigger type |
| ---------: | ------ | ------------------------------------------------------------ | -----------: |
| `0` (`00`) | `LEG`  | Original PriorityEncoder output; fixed length from `Pkt Len` |       `0x08` |
| `1` (`01`) | `RAW`  | Complete uncompressed LadderWrapper event                    |       `0x04` |
| `2` (`10`) | `COMP` | Clustered data only; variable length                         |       `0x02` |
| `3` (`11`) | `MIX`  | Complete RAW event followed by clustered data                |       `0x01` |

All four `DAQ_MODE` values are valid with `cal`, `daq` and run mode `mix`.

`cal` never emits normal events, regardless of `DAQ_MODE`.

After calibration, `daq` and run mode `mix` return to the configured `DAQ_MODE`.

---

## Calibration table packets

Calibration tables always use the 16-to-32-bit packet, independently of `DAQ_MODE`.

| Table     | Packet type |
| --------- | ----------: |
| Pedestal  |      `0x80` |
| Raw sigma |      `0x40` |
| Sigma     |      `0x20` |
| Flags     |      `0x10` |

---

## Old command compatibility

```text
startOCA cal [runnum]   -> cal 0 save
startOCA beam [runnum]  -> daq 1 nosave
startOCA mix [runnum]   -> mix 0 save

startOCA 0 [runnum]     -> cal 0 save
startOCA 1 [runnum]     -> daq 1 nosave
startOCA 2 [runnum]     -> mix 0 save
```

---

## Extended 16-bit control field

|   Bits | Meaning                                           |
| -----: | ------------------------------------------------- |
|   `15` | Extended-format marker                            |
|    `3` | `save`                                            |
|    `2` | Trigger source: `0` internal, `1` external        |
|  `1:0` | Run mode: `00 cal`, `01 daq`, `10 mix`, `11 dump` |
| `14:4` | Reserved; must be zero                            |

Accepted legacy wire codes:

| Code           | Meaning        |
| -------------- | -------------- |
| `0000`, `0004` | `cal`          |
| `0002`         | `beam` / `daq` |
| `0001`         | `mix`          |

---

## FPGA register interface

At every non-dump start, OCA first writes:

```text
REG11 = HTH[31:16] | LTH[15:0]
```

It then writes one complete atomic command to `REG0`.

### `REG0` fields

|    Bits | Field          |
| ------: | -------------- |
|     `0` | `DETECTOR_INTERFACE_RESET` |
|     `1` | `COUNTER_RESET` |
|     `2` | `REGISTER_ARRAY_RESET` |
|     `4` | `RUN_REQUEST`  |
|    `16` | `EVENT_ENABLE` |
|    `17` | `FORCE_CALIB`  |
|    `18` | `THR_VALID`    |
|    `19` | `AUTO_CALIB`   |
|    `20` | `SAVE_CALIB`   |
| `25:24` | `DAQ_MODE`     |

Bits `0`, `1` and `2` generate reset pulses and remain clear in a normal run command.

### Readback slot 11

| Bit | Field       |
| --: | ----------- |
| `0` | `CAL_VALID` |
| `1` | `RUN_IDLE`  |

The HPS readback bank starts at offset `16`, so slot `11` is read at address:

```text
16 + 11 = 27
```

Writable `REG11` and readback slot `11` are separate registers in opposite-direction banks.

---

## `stopOCA` sequence

`stopOCA` performs the following sequence:

1. clear `REG0.RUN_REQUEST`;
2. allow PAPERO to finish the accepted event or table;
3. wait for `RUN_IDLE` on every board;
4. stop the HPS senders;
5. perform final FIFO reads;
6. close the MAKA run.

PAPERO keeps metadata and output paths active until the current event or table is complete.

OCA polls `RUN_IDLE` for up to 30 seconds. On timeout, `stopOCA` returns an error while the HPS senders and MAKA remain active, allowing a later stop retry.

### Stop during calibration

| Stop point                                      | Result                                                                                 |
| ----------------------------------------------- | -------------------------------------------------------------------------------------- |
| Between calibration triggers                    | Calibration is aborted; calibration RAM is not reset                                   |
| During table serialization from new calibration | The current table is completed before abort; no partial packet crosses the run boundary |
| During a standalone cached dump                  | All four cached tables are completed before the run stops                               |

# Data Format of Files Produced by MAKA

## MAKA File Header

<table class="tg">
<thead>
  <tr>
    <th class="tg-vxga">31</th>
    <th class="tg-vxga">30</th>
    <th class="tg-vxga">29</th>
    <th class="tg-vxga">28</th>
    <th class="tg-vxga">27</th>
    <th class="tg-vxga">26</th>
    <th class="tg-vxga">25</th>
    <th class="tg-vxga">24</th>
    <th class="tg-vxga">23</th>
    <th class="tg-vxga">22</th>
    <th class="tg-vxga">21</th>
    <th class="tg-vxga">20</th>
    <th class="tg-vxga">19</th>
    <th class="tg-vxga">18</th>
    <th class="tg-vxga">17</th>
    <th class="tg-vxga">16</th>
    <th class="tg-vxga">15</th>
    <th class="tg-vxga">14</th>
    <th class="tg-vxga">13</th>
    <th class="tg-vxga">12</th>
    <th class="tg-vxga">11</th>
    <th class="tg-vxga">10</th>
    <th class="tg-vxga">9</th>
    <th class="tg-vxga">8</th>
    <th class="tg-vxga">7</th>
    <th class="tg-vxga">6</th>
    <th class="tg-vxga">5</th>
    <th class="tg-vxga">4</th>
    <th class="tg-vxga">3</th>
    <th class="tg-vxga">2</th>
    <th class="tg-vxga">1</th>
    <th class="tg-vxga">0</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td class="tg-h47o" colspan="32">Known word:   0xB01ADEEE</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">UNIX time of the run</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">MAKA git hash</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="4">Type</td>
    <td class="tg-h47o" colspan="12">Data Version</td>
    <td class="tg-h47o" colspan="16" rowspan="2">Boards connected</td>
  </tr>
  <tr>
    <td class="tg-h47o">tc</td>
    <td class="tg-h47o">cal</td>
    <td class="tg-h47o">hk</td>
    <td class="tg-h47o">sc</td>
    <td class="tg-h47o" colspan="4">Major</td>
    <td class="tg-h47o" colspan="4">Minor</td>
    <td class="tg-h47o" colspan="4">Patch</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="16">Board ID 1</td>
    <td class="tg-h47o" colspan="16">Board ID 0</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="16">Board ID N-1</td>
    <td class="tg-h47o" colspan="16">…</td>
  </tr>
</tbody>
</table>

## MAKA Event Header + Payload

<table class="tg">
<thead>
  <tr>
    <th class="tg-vxga">31</th>
    <th class="tg-vxga">30</th>
    <th class="tg-vxga">29</th>
    <th class="tg-vxga">28</th>
    <th class="tg-vxga">27</th>
    <th class="tg-vxga">26</th>
    <th class="tg-vxga">25</th>
    <th class="tg-vxga">24</th>
    <th class="tg-vxga">23</th>
    <th class="tg-vxga">22</th>
    <th class="tg-vxga">21</th>
    <th class="tg-vxga">20</th>
    <th class="tg-vxga">19</th>
    <th class="tg-vxga">18</th>
    <th class="tg-vxga">17</th>
    <th class="tg-vxga">16</th>
    <th class="tg-vxga">15</th>
    <th class="tg-vxga">14</th>
    <th class="tg-vxga">13</th>
    <th class="tg-vxga">12</th>
    <th class="tg-vxga">11</th>
    <th class="tg-vxga">10</th>
    <th class="tg-vxga">9</th>
    <th class="tg-vxga">8</th>
    <th class="tg-vxga">7</th>
    <th class="tg-vxga">6</th>
    <th class="tg-vxga">5</th>
    <th class="tg-vxga">4</th>
    <th class="tg-vxga">3</th>
    <th class="tg-vxga">2</th>
    <th class="tg-vxga">1</th>
    <th class="tg-vxga">0</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td class="tg-h47o" colspan="32">Known word:   0xFA4AF1CA</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">UTC seconds [31:0]</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">UTC seconds [63:32]</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">UTC nanoseconds [31:0]</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">UTC nanoseconds [63:32]</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">Precomputed event length in bytes</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="32">Event Number</td>
  </tr>
  <tr>
    <td class="tg-h47o" colspan="4">Type</td>
    <td class="tg-h47o" colspan="12">Status</td>
    <td class="tg-h47o" colspan="16">Boards in the event</td>
  </tr>
  <tr>
    <td class="tg-vhtn" colspan="32">Payload 0</td>
  </tr>
  <tr>
    <td class="tg-vhtn" colspan="32">…</td>
  </tr>
  <tr>
    <td class="tg-vhtn" colspan="32">Payload N-1</td>
  </tr>
</tbody>
</table>

## PAPERO Event

<table class="tg">
<thead>
  <tr>
    <th class="tg-nrix">31</th>
    <th class="tg-vxga">30</th>
    <th class="tg-vxga">29</th>
    <th class="tg-vxga">28</th>
    <th class="tg-vxga">27</th>
    <th class="tg-vxga">26</th>
    <th class="tg-vxga">25</th>
    <th class="tg-vxga">24</th>
    <th class="tg-vxga">23</th>
    <th class="tg-vxga">22</th>
    <th class="tg-vxga">21</th>
    <th class="tg-vxga">20</th>
    <th class="tg-vxga">19</th>
    <th class="tg-vxga">18</th>
    <th class="tg-vxga">17</th>
    <th class="tg-vxga">16</th>
    <th class="tg-vxga">15</th>
    <th class="tg-vxga">14</th>
    <th class="tg-vxga">13</th>
    <th class="tg-vxga">12</th>
    <th class="tg-vxga">11</th>
    <th class="tg-vxga">10</th>
    <th class="tg-vxga">9</th>
    <th class="tg-vxga">8</th>
    <th class="tg-vxga">7</th>
    <th class="tg-vxga">6</th>
    <th class="tg-vxga">5</th>
    <th class="tg-vxga">4</th>
    <th class="tg-vxga">3</th>
    <th class="tg-vxga">2</th>
    <th class="tg-vxga">1</th>
    <th class="tg-vxga">0</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td class="tg-3ygc" colspan="32">Known word:   0xBABA1A9A</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">Length</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">Detector git hash</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">Trigger number</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="16">Detector ID</td>
    <td class="tg-3ygc" colspan="16" rowspan="2">Trigger ID</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="8">Type</td>
    <td class="tg-3ygc" colspan="8">Progressive number</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">Internal timestamp iINT_TS [31:0]</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="1">1</td>
    <td class="tg-3ygc" colspan="7">Reserved</td>
    <td class="tg-3ygc" colspan="8">SSID</td>
    <td class="tg-3ygc" colspan="8">Reserved</td>
    <td class="tg-3ygc" colspan="8">Packet type</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">External Timestamp [63:32]</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">External Timestamp [31:0]</td>
  </tr>
  <tr>
    <td class="tg-vxga" colspan="32">Payload 0</td>
  </tr>
  <tr>
    <td class="tg-vxga" colspan="32">...</td>
  </tr>
  <tr>
    <td class="tg-vxga" colspan="32">Payload Length-11</td>
  </tr>
  <tr>
    <td class="tg-i93t" colspan="32">Known word: 0x0BEDFACE</td>
  </tr>
  <tr>
    <td class="tg-i93t" colspan="32">CRC-32, init: 0xFFFFFFFF</td>
  </tr>
</tbody>
</table>