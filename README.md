# udptransfer

Workflow total :
1. Read data, masuk ke send buffer 
2. Kirim data di send buffer
3. Receiver terima data, masuk buffer, kirim ack
4. Hapus data di send buffer

Workflow program :
- Send File
1. Ambil parameter dari args
2. Buka socket, pasang port ke socket
3. Baca file, tiap 1 byte (1 char) masukin ke dalam frame, masukin ke buffer (seqnum ?)
4. Kirim, sampe buffer penuh lalu tunggu ACK
5. Tunggu ACK, kalau ACK sudah ketemu hapus di buffer (implementasi timeout?)
6. Kalau sudah dapat ACK, kirim frame selanjutnya
7. Kalau file sudah habis kirim EOF(?)
8. Kalau dapat dupack / ga dapet ACK, kirim ulang 1 buffer
9. Logging

- Receive File
1. Tunggu file, kalau dapat masuk ke buffer, kirim ACK
2. Kalau ada seqnum yang miss, kirim dupack
3. Kalau buffer mau penuh, flush ke file

Work Breakdown Structure :
- Send file :
	- Algoritma checksum
	- Ambil data dari file
	- Read, masukin ke frame
	- Tunggu ACK, dan segala cabangnya
	
- Recv file
	- Hitung checksum dari file yang diterima
	- Kirim ACK
	- Kirim DUPACK
	- Flush ke File
	
- Refactoring
	
Question :
- Recv file berhenti ketika file selesai ditransmit semua?
