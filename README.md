# Sliding Window
Tugas 1 IF3130 - Jaringan Komputer (Semester 1 2017/2018)

## Oleh:
- Oktavianus Handika - 13515035
- Felix Limanta - 13515065
- Vincent Hendryanto Halim - 13515089

## Petunjuk Penggunaan Program
1. Jalankan Makefile untuk kompilasi file
    `make`
2. Jalankan program penerima.
    `./sendfile <filename> <windowsize> <buffersize> <destination_ip> <destination_port>`
3. Jalankan program pengirim
    `./recvfile <filename> <windowsize> <buffersize> <port>`

## Cara Kerja Program
### sendfile
Secara garis besar, *sendfile* dibagi menjadi 3 *thread*: *sender*, *receiver*, dan *timeout*.

*Thread* akan melakukan *load* suatu *file* pada *buffer*, dibagi sesuai ukuran *buffer* yang diberikan. Jika seluruh data pada *buffer* sudah terkirim, *file* akan dibaca lagi. Ini dilakukan sampai seluruh *file* terbaca. Setelah *file* terbaca, secara berurutan data dimasukkan ke *segment* dan diletakkan di *window buffer*. Ukuran *window buffer* sebesar *window size*. *Segment* kemudian akan dikirim, lalu nilai LFS ditambah. Jika LFS - LAR > SWS, pembuatan *segment* dari data akan ditunda sampai LAR bertambah.

*Thread receiver* akan menunggu ACK dari data yang dikirim. LAR akan diinkremen sesuai dengan ACK yang diterima. Jika ada nilai ACK yang tidak diterima, tetapi ACK setelahnya diterima, data yang direpresentasikan ACK yang tidak diterima tersebut akan dianggap sudah diterima.

*Thread timeout* bertanggung jawab memantau *timeout* setiap *segment* yang telah dikirim pada *window buffer*. Jika *timeout* sudah habis, status pengiriman akan dibatalkan dan *segment* akan dikirim ulang oleh *thread sender*.

Setelah semua data pada *file* dikirim, akan dikirim *segment* EOF oleh *thread sender*, yang diperlakukan mirip dengan *segment* biasa. Ketika *thread receiver* menerima ACK dari EOF, eksekusi semua *thread* akan diberhentikan dan program selesai.

## Penerima
Receivefile akan menunggu *segment* yang akan diterima, jika *segment* berada pada *window* (>= *LastFrameReceived* (LFR) dan <= *LargestAcceptableFrame* (LAF)) dan *checksum*-nya valid, maka *segment* akan dimasukkan ke dalam *buffer* meskipun bukan *next sequence* yang diharapkan. ACK untuk *nextSequence* saat itu akan dikirim.

Jika *segment* yang dikirim merupakan *segment* yang diharapkan (nextSeq) maka nilai LFR, LAF, dan nextSeq akan dimajukan menuju tempat dimana *buffer* masih kosong.

Jika terdapat *file* yang terlewat, akan dikirim nextSeq dengan isi nextSeq adalah *file* pertama yang terlewat.

## Pembagian Tugas
- **Frame + Checksum:** 13515035 - Oktavianus Handika
- **Send:** 13515065 - Felix Limanta
- **Receive:** 13515089 - Vincent Hendryanto Halim

## Jawaban Pertanyaan
### 1. Apa yang terjadi jika advertised window yang dikirim bernilai 0? Apa cara untuk menangani hal tersebut?

Ketika *advertised window size* = 0, *sender* tidak dapat mengirim paket apapun kepada *receiver*. Akibatnya, *receiver* mungkin saja tidak dapat mengirimkan paket (ACK) kepada *sender* bila saat itu *receiver* masih menunggu menerima paket dari *sender*. Akibatnya, *sender* tidak bisa mengirim paket kepada *receiver* karena tidak menerima ACK dari *receiver*. Sehingga, ketika *advertised window size* = 0, dapat menimbulkan risiko *dead-lock* antara *sender* dengan *receiver* karena tidak ada transfer paket.

Untuk menangani hal tersebut, ketika *sender* memiliki sejumlah data pada *send buffer*, *sender* secara periodik akan mengirim 1-byte *segment* kepada *receiver* agar *receiver* melakukan respon kepada *sender*. *Receiver* akan mengirim ACK dengan *advertised window size* dengan suatu nilai bukan 0 yang dapat digunakan oleh *sender* untuk melakukan transimisi.

### 2. Sebutkan *field* data yang terdapat TCP Header serta ukurannya, ilustrasikan, dan jelaskan kegunaan dari masing-masing *field* data tersebut!
*Field* data pada TCP Header:
```
    0                   1                   2                   3   
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Source Port          |       Destination Port        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sequence Number                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Acknowledgment Number                      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Data |           |U|A|P|R|S|F|                               |
   | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
   |       |           |G|K|H|T|N|N|                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           Checksum            |         Urgent Pointer        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Options                    |    Padding    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                             data                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- __*Source port*__ (16 bit): angka dari *source port*, merupakan sebuah alamat global dari mana segmen dikirimkan
- __*Destination port*__ (16 bit): angka dari port tujuan, merupakan sebuah alamat global ke mana segmen dikirimkan
- __*Sequence number*__ (32 bit): nomor urut dari oktet pertama dari data dalam sebuah *segment* yang akan dikirimkan.
- __*Acknowledgement number*__ (32 bit): nomor urut dari oktet selanjutnya dalam sebuah *segment* untuk diterima oleh *sender* dari *receiver* pada pengiriman selanjutnya.
- __*Data offset*__ (4 bit): mengindikasikan di mana data dalam *segment* dimulai.
- __*Reserved*__ (6 bit) : bit yang direservasi untuk penggunaan lain waktu. *Sender* akan mengeset nilai-nilai bit ini ke nilai 0.
- __*Flags*__ (6 bit) : Mengindikasikan *flag* yang ada pada TCP, yaitu URG (*Urgent*), ACK (*Acknowledgment*), PSH (*Push*), RST (*Reset*), SYN (*Synchronize*), dan FIN (*Finish*).
- __*Window*__ (16 bit) : jumlah byte yang tersedia yang dimiliki oleh *buffer host receiver* yang bersangkutan (dalam hal ini *Receive buffer*).
- __*Checksum*__ (16 bit) : pemeriksaan *segment integrity* (*header* dan *payload*-nya). Nilai *field Checksum* akan diatur ke nilai 0 selama proses kalkulasi *checksum*.
- __*Urgent Pointer*__ (16 bit) : Menandai lokasi data pada *segment* yang dianggap *urgent*.
- __*Options*__ (banyak bit bervariasi) : menampung beberapa opsi tambahan TCP. Satu opsi TCP dapat memakan 32 bit sehingga ukuran *header* TCP dapat diindikasikan dengan menggunakan *data field offset*.
- __*Padding*__ (banyak bit bervariasi) : membuat TCP *header* berakhir dan memulai suatu data pada bit 32.
