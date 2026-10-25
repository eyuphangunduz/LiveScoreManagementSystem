# Live Score Management System

A client-server **sports league and live score system** written in **C++**, built on **Berkeley Sockets** over **TCP**.
The server manages an 8-team league, simulates matches minute by minute and broadcasts live events to the client.
Every message travels inside a custom binary packet protected by a **checksum**, and a simulated **noisy channel**
with **Stop-and-Wait ARQ** demonstrates error detection, error correction and the relationship between **BER and SNR**.

Developed for the **Data Communication** course — Eyüphan Gündüz, Arel University, Computer Engineering.

---

## Features

| Course requirement | How it is implemented |
|---|---|
| **Socket programming** | Berkeley Sockets API: `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv` |
| **Protocol and services** | TCP (connection-oriented). See [Why TCP?](#why-tcp) |
| **Encode / decode data into packets** | Custom binary frame: `[size (4 B) \| payload (N B) \| checksum (2 B)]`, big-endian |
| **Error control (detect and correct)** | 16-bit Internet Checksum (RFC 1071) detects errors, **Stop-and-Wait ARQ** (ACK/NACK + retransmission) corrects them |
| **BER vs SNR (dB)** | BPSK noisy channel model, `BER = ½·erfc(√SNR)`, with a measurement experiment |

Other features:
- 8-team **round-robin fixture** (7 weeks, 28 matches)
- Minute-by-minute **match simulation** with live goal events
- League table sorted by points, goal difference and goals scored
- **Persistent storage** in `league_data.txt` (the server resumes where it stopped)
- **Season reset** protocol
- Interactive client menu with a **two-way command protocol**
- Adjustable channel noise (SNR) during a live session

---

## Build

The code uses only the C++ standard library and POSIX/Berkeley socket headers. It builds on **Linux** and **macOS** without
any extra libraries. On **Windows**, use **WSL** (Windows Subsystem for Linux).

### Linux / WSL
```bash
sudo apt install g++ cmake git
git clone https://github.com/eyuphangunduz/LiveScoreManagementSystem.git
cd LiveScoreManagementSystem
cmake -B build
cmake --build build
```

### macOS
```bash
xcode-select --install          # C++ compiler (one-time)
brew install cmake              # optional
git clone https://github.com/eyuphangunduz/LiveScoreManagementSystem.git
cd LiveScoreManagementSystem
cmake -B build
cmake --build build
```

### Without CMake (any platform)
```bash
g++ -std=c++20 LiveScoreManagementSystem.cpp -o LiveScoreManagementSystem
```
(Use `clang++` instead of `g++` on macOS.)

### Windows
1. Open PowerShell as administrator and run `wsl --install`, then restart.
2. Open **Ubuntu** and follow the Linux steps above.

---

## Usage

Open **two terminals** in the project folder.

**Terminal 1 — server**
```bash
./build/LiveScoreManagementSystem server
```

**Terminal 2 — client**
```bash
./build/LiveScoreManagementSystem client
```

(If you built without CMake, use `./LiveScoreManagementSystem` instead of `./build/LiveScoreManagementSystem`.)

### Client menu
| Option | Action |
|---|---|
| 1 | Play the next week live (goals arrive in real time) |
| 2 | View the league standings |
| 3 | View next week's fixture |
| 4 | Reset the season |
| 5 | Set the channel SNR in dB (5–30), or `off` for a perfect channel |
| 6 | Exit |

**Tip:** choose option 5 and enter `8`, then option 1. You will see corrupted packets being detected by the checksum
and retransmitted by ARQ, followed by a transmission summary such as:
```
Transmission summary: 26 packets, 2 retransmissions, 0 lost.
```

### BER vs SNR experiment
```bash
./build/LiveScoreManagementSystem ber
```
Sends 5000 packets through the noisy channel for every SNR value from 0 to 10 dB and prints the theoretical and measured
BER, and how many corrupted packets the checksum detected:
```
SNR(dB)   Theoretical BER Measured BER  Corrupted   Detected  Undetected
0         7.86e-02        7.90e-02      5000        5000      0
4         1.25e-02        1.25e-02      4992        4981      11
8         1.91e-04        1.86e-04      453         453       0
10        3.87e-06        4.30e-06      11          11        0
```
The measured BER closely matches the theory. The checksum detects all single-bit errors; the few undetected packets
appear at medium SNR, where two bit errors can cancel each other out in the one's-complement sum — a known weakness of
the Internet Checksum and the reason real links use CRC.

---

## Protocol

### Packet format
```
+----------------+---------------------+--------------------+
| size (4 bytes) |  payload (N bytes)  | checksum (2 bytes) |
+----------------+---------------------+--------------------+
```
- **size** — payload length, big-endian. It lets the receiver know where each message ends (framing over a TCP byte stream).
- **payload** — the message text (a goal event, the standings, a command...).
- **checksum** — 16-bit Internet Checksum of the payload.

### Commands (client → server)
`PLAY`, `STANDINGS`, `FIXTURE`, `RESET`, `SNR <dB>` / `SNR off`, `EXIT`

Each server response ends with a special `#END#` packet, so the client knows when to show the menu again.

### Error control (server → client)
1. The server sends a packet through the noisy channel and waits.
2. The client recalculates the checksum: valid → `ACK`, corrupted → `NACK`.
3. On `NACK`, the server retransmits the same packet (up to 20 times).

---

## Why TCP?

- **Connection-oriented session:** the client and server hold a conversation (menu → command → response), which fits a TCP connection naturally.
- **Reliable, in-order delivery:** live events must arrive in the right order (a goal cannot appear before kick-off) and none may be lost, or the score shown would be wrong.
- **Byte stream:** TCP does not preserve message boundaries, so the protocol adds its own length field (framing).

TCP already protects data with its own checksum and retransmissions, so on a real TCP connection the application never
sees a corrupted packet. The **noisy channel** is therefore simulated at the application layer, on top of TCP, to
demonstrate how error detection and correction work on an unreliable physical link.