# bewegungsmelder
Raspberry Pi Zero kullanarak ortam hareketi algılama IoT mini projesi

<b>Çalıştırmak için: </b><br>
* Öncelikle Raspberry Pi Zero ile HC-SR501 PIR Sensörün bağlantısını doğru bir şekilde oluşturun:
![image](https://github.com/narkolepsizm/bewegungsmelder/assets/29272356/d143aa86-534d-4542-bec3-95318b8f372d)
* Raspberry Pi Zero'ya gerekli kütüphaneleri kurun. Uygulamanın çalışması için gerekli olan kütüphaneler aşağıda listelenmiştir:
  * stdio.h
  * stdbool.h
  * bcm2835.h
  * time.h
  * sqlite3.h
  * microhttpd.h
  * string.h
  * cjson/cJSON.h
  * curl/curl.h
* Uygulamayı derleyin:
```console
gcc bewegungsmelder.c -o bewegungsmelder -lbcm2835 -lsqlite3 -lcjson -lmicrohttpd -lcurl
```
* Uygulamayı çalıştırın:
```console
./bewegungsmelder
```
* Hareket gerçekleştiğinde terminalde bununla ilgili çıktı bulabilirsiniz.
* Projenin IoT kısmı için bir veritabanı ve bir sunucu uygulama içerisinde halihazırda mevcuttur. Veritabanındaki bilgileri işleyebilecek bir arayüz oluşturarak bu bilgilere(hareket geçmişi, anlık hareket durumu) uygulamadaki sunucu aracılığıyla internet üzerinden ulaşabilirsiniz. 
* Projede ayrıca OneSignal REST API entegrasyonu mevcuttur. OneSignal bilgilerinizi(```ONESIGNAL_API_KEY``` ve ```ONESIGNAL_APP_ID```) uygulamaya girerek hareket oluşması durumunda bildirim alıp anında haberdar olabilirsiniz. 

