# ONKYO automaticc turn on when powered the amp
TX-NX7SR

Onkyo FR-N7SX(X-N7SX)

Send TAPE turn on signal 10 times.

|IR CODE| Function|
|---|---|
|0x7F | TAPE turn on|



## Circuit

Use PD7 port.

nSRST function should be disabled by WCH debugger.
Simply PD7 and register(200Ω) output is connected to Onkyo RI port. 5V TTL output. No transiter required.

<img width="401" height="319" alt="image" src="https://github.com/user-attachments/assets/c46ecdf4-1671-424a-8546-13b14e02abaa" />

<img width="816" height="923" alt="image" src="https://github.com/user-attachments/assets/ab609be9-3298-4fe3-95f2-9353b06db7d7" />
