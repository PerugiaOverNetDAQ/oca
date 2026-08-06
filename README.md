# Calibration data injection

Arm a one-shot calibration from a LEGACY or MIXED `.dat` file with:

```text
startOCA cal int save --inject /path/to/input.dat
```

Only the LEGACY payload is injected.  The frontend FIFOs are still drained
and discarded during injection.  At the end of calibration injection is
disabled; if the file does not contain enough events, OCA reports the
underflow and restarts the complete calibration using frontend data.

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
    <td class="tg-h47o" colspan="32">Length in bytes</td>
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
    <td class="tg-3ygc" colspan="32">Internal Timestamp [63:32]</td>
  </tr>
  <tr>
    <td class="tg-3ygc" colspan="32">Internal Timestamp [31:0]</td>
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
