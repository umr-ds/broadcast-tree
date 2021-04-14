# Broadcast Tree Protocol

## Frame Format

```
 0                   1                   2                   3  
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Flags|Un.| Type|                    Game ID                    |
+-+-+-+-+-+-+-+-+                               +-+-+-+-+-+-+-+-+
|                                               |   Sequence #  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               |                       TX                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               |                 Parent Address                |
+-+-+-+-+-+-+-+-+                               +-+-+-+-+-+-+-+-+
|                                               |               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Highest TX                  |               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 2nd Highest TX                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

With the dollowing flags:
- `Receive Error`
- `End of Game`
- `Mutex update`

### Frame Types:

- Cycle Check Frame:
  - Ping-To-Source
  - Mutex
  - Path-To-Source
- Neighbor Discovery Frame
- Child Request Frame
- Child Confirmation Frame
- Child Rejection Frame
- Parent Revocation Frame
- End of Game Frame
- Application Data Frame 

## Transmission strength

When receiving a frame, compute `required Power` ($rP$) to transmit to sender 

$rP = txPower − (snr − minSNR)$,

where:
- $txPower$: maximum transmit power of this node
- $minSNR$: minimum signal-to-noise ration as specifided by transmission standard

$snd = rxPower − noisePower$

where:
- $rxPower$: received signal strength
- $noisePower$: 

<!--- Ich hoffe, das bekommen wir alles vom Betriebssystem... --->

## Tree Creation

1. Neighbour Discovery:

- Always sent at maximum power
- When receiving
```
+---------------+                  +------------------------------+
|               |  Is Source       |                              |
|  Do Nothing   |<-----------------+ Receive Neighbour Discovery  |
|               |                  |                              |
+---------------+                  +---------------+--------------+
       ^      ^                                    |
       |      |                                    |not source
       |      |                                    |
       |      |                                    v
       |      |                         +--------------------+
       |      |      No                 |                    |
       |      +-------------------------+  Reduce parent rT  |
       |                                |  by disconnecting  |
       |                                |                    |
       |                                +----------+---------+
       |                                           |
       |                                           |yes
       |                                           |
       |                                           v
       |                          +----------------------------------+
       |             No           |                                  |
       +--------------------------+ Old parent rT decrease greater   |
                                  | or equal to new parent increase  |
                                  |                                  |
                                  +----------------+-----------------+
                                                   |
                                                   |yes
                                                   v
                                         +-----------------------+
                                         | Switch to new parent  |
                                         +-----------------------+
```

<!--- Was, wenn wir mehrere gleichgute potentielle Eltern haben? Konvergiert das Protokoll dan neinfach niemals? --->

2. Parent Switching:

If child has not finished game: <!--- Warum? Wenn das spiel zuende ist, sollten doch keine Änderungen mehr passieren. --->
- Child sends `Parent Revocation Frame` to old parent
  - Adds old parent to internal list of `Previous Parents`
- Old parent: 
  - Removes former child from children -list
  - Recomputes largest and second-largest transmission power
  - Sends `Neighbor Discovery Frame` at max power
  - Resets `Unchanged Round Counter`
- Child sends `Child Request Frame` to new parent
- New parent:
  - Checks if child is "sufficienty close" <!--- Wie? Geht es da um die benötigte tx vs maximale tx? --->
  - Checks if child is own parent
  - Checks if it has connection to source <!--- Wie? --->
    - If all ok: `Sends Child Confirmation Frame`
    - Else: Sends  `Child Rejection Frame`

If new parent accepts connection:
- Child checks again, if better parent exists
  - If yes: switch again
  - If no: send `Neighbor Discovery Frame` at max power
- If parent has finished game:
  - Child finishes game, sends `End of Game Frame`

If new parent refuses connection:
- Increments `Rejected Connection Counter`
- Adds node to `Blacklist`
If `Rejected Connection Counter` not at max:
  - Try another parent <!--- Wie? --->
Else:
- Child disconnects all own children <!--- Wie? Mit einem Child Rejection Frame? --->
- Waits for new `Neighbor Discovery Frame`
- Goes through list of `Previous Parents`<!--- Warum müssen wir dann erst auf einen Discovery frame warten? --->
  - Reject former parent if:
    - Too far away now <!--- Bestimmt anhand von was? --->
    - Has become a child <!--- Im Zweifel nur, wenn es ein Kind dieses Knotens geworden ist, oder? --->
  - Choose newest, still valid parent to reconnect
- If no valid, previous parent exists
  - Choose from other peers <!--- Nochmal, warum nicht von anfang an so? --->
- If no possible parents exist:
  - Wait for new `Neighbor Discovery Frame`

If node has finished game <!--- Wie? --->
- Send `End of Game Frame`to parent
If all children have finished AND `Unchanged Round Counter` at max:
- Parent finishes game

## Loop Prevention

### Ping-to-Source

Send packet to source, if we ever see it again, then ther's a loop somewhere

<!--- Wie lösen wir den loop dann auf? --->

### Path-to-source

Every node announces the entire path from itself to the source in every broadcast

### Mutex

A cacle happens when we connect to a node from our own subgraph -> freeze subgraph before connecting.
Unfreeze after connection has bee established

<!--- Wie stelle nwir sicher, dass unser freeze im gesamten subbaum angekommen ist? --->
<!--- Was wenn ein kind den unfreeze nicht bekommt? --->

## Ungeklärt:

- Zähler für unveränderte Runden (`Unchanged Round Counter`) <!--- Wo/wann wird der inkrementiert? --->
  - Reset by parent if child disconnects
  - Reset by parent if new child accepted
  - Reset by child when connection is accepted
- Zähler für zurückgewiesene Verbindungen (`Rejected Connection Counter`)
  - Reset by child when connection is accepted
  - Incremented by child when connection is refused
