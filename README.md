Rigol Oscilloscope GUI Controller

โปรแกรม GUI สำหรับควบคุมเครื่องออสซิลโลสโคป (Oscilloscope) ยี่ห้อ Rigol พัฒนาด้วยภาษา C++17 และ 
Qt Framework ผ่านโปรโตคอล SCPI รองรับทั้งการเชื่อมต่อฮาร์ดแวร์จริงและการทำงานในโหมดจำลอง (Simulation)

ฟีเจอร์หลัก (Key Features)

Dual Mode Operation:

-Simulation Mode: ทดสอบฟังก์ชัน แสดงรูปคลื่นจำลองได้ทันทีโดยไม่ต้องเชื่อมต่ออุปกรณ์จริง
-Real Hardware Mode: เชื่อมต่อกับเครื่อง Rigol ผ่าน USB หรือ LAN โดยใช้ไลบรารี NI-VISA
-Asynchronous Screen Capture: ดึงภาพหน้าจอจากเครื่องออสซิลโลสโคปผ่าน Background Thread (QThread) 
เพื่อป้องกัน UI ค้างขณะโหลดภาพ พร้อมฟังก์ชันบันทึกภาพเป็นไฟล์ .png
-Interactive Control: ควบคุมการเปิด/ปิด สัญญาณช่อง CH1 – CH4 และปรับค่า Volt/Div หรือ Time/Div ได้แบบเรียลไทม์
-SCPI Command Terminal: ส่งคำสั่ง SCPI และรับผลลัพธ์การตอบกลับ 
พร้อมปุ่มทางลัดคำสั่งมาตรฐาน (*IDN?, :RUN, :STOP, :AUToscale, :SINGle)

ความต้องการของระบบ (Prerequisites)

Compiler: g++ หรือ clang ที่รองรับมาตรฐาน C++17 ขึ้นไป
Build System: CMake (เวอร์ชัน 3.16 ขึ้นไป) และ Make / Ninja
Qt Framework: Qt 5 หรือ Qt 6 (โมดูล Core, Gui, Widgets)
VISA Driver (เฉพาะโหมดเชื่อมต่อเครื่องจริง): libvisa-dev (Linux) หรือ NI-VISA Runtime (Windows)

