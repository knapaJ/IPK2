# IPK2 projekt - Simple File Transfer Protocol

## Kompilace

Kompilace je možná pomocí přiloženého souboru `Makefile` a programu `GNU make`, nebo pomocí `CMake`.
Za potřebí je mít standardní kompilátory jazyků `C` a `C++`.

## Použití

Spuštění zkompilovaných souborů je možno provést následovně:

### Server
```bash
./ipk-simpleftp-server -f <work-dir> -u <user-db> [-i <interface-name>] [-p <port>]
```
Výchozí port je 115; výchozí chování bez nastavení rozhraní je poslouchání na všech dostupných rozhraních.

Pro bližší podrobnosti viz [manuál](./manual.pdf).

### Klient
```bash
./ipk-simpleftp-client -f <work-dir> -h <server-address/hostname> [-p <server-port>]
```
Výchozí port je 115, pokud není zadán jiný.

Pro bližší podrobnosti a návod na použití klientské aplikace viz [manuál](./manual.pdf).

## Implementační omezení

Jediným mě známým implementačním omezením jest skutečnost, že server dokáže najednou obsluhovat pouze jednoho klienta.
Další klienti budou hostitelkým systémem zařazeni do fronty, nebo odmítnuti.