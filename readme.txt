# ISO-TP Multithreaded CAN Parser

This project implements a multithreaded parser for ISO-TP (ISO 15765-2) transport layer messages transmitted over raw CAN frames.

It reads frames from a plain text file and reconstructs complete ISO-TP messages by interpreting the different frame types: Single Frame (SF), First Frame (FF), Consecutive Frame (CF), and Flow Control (FC). Output is ordered according to the original file.

---

## 📂 Files

- `iso_tp_threadpool_ordered.cpp` – Main source file
- `transcript.txt` – Input file containing CAN frames (one per line)

---

## 🛠 Compilation (Clang++ on macOS)  

Make sure you have Clang(works with every compilator and OS) and a C++17-compatible environment. Then run:

```bash
clang++ -std=c++17 -pthread -o iso_tp_parser par.cpp

./iso_tp_parser
