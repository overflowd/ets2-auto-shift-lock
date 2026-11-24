# ETS2 Otomatik Vites Fren Kilidi Eklentisi — Tasarım Dokümanı

**Tarih:** 2026-11-24  
**Durum:** Kullanıcı onayına hazır

## 1. Amaç ve Motivasyon

Euro Truck Simulator 2 (ETS2) "Gerçek Otomatik" (`Real Automatic`) şanzıman modunda, oyuncular fren pedalına basmadan ve hatta araç hareket halindeyken `D`, `N` ve `R` arasında geçiş yapabilmektedir. Gerçek otomatik vitesli araçlarda ise vites kilidi (shift-lock) mekanizması, sürücü frene basmadan ve araç tamamen durmadan `D`, `N` ve `R` arasında geçiş yapılmasını engeller.

Bu projenin amacı; sadece direksiyon seti kullanıcıları için değil, **klavye, gamepad ve tüm direksiyon/vites donanımlarıyla** uyumlu, herkesin kolayca kurup arka planda ekstra uygulama çalıştırmadan kullanabileceği bir ETS2 eklentisi geliştirmektir.

## 2. Hedefler, Hedef Dışı Konular ve Anti-Hedefler

### Hedefler
- ETS2 `win_x64/plugins` klasörüne bırakılan tek bir native `x64` DLL (`ets2_auto_shift_lock.dll`) ile oyun içinde doğrudan çalışmak.
- **Şanzıman moduna göre akıllı davranış (Kullanıcı Kararı — 1A):**
  - `Gerçek Otomatik` (`shifter.type == "automatic"`): Fren + tam durma kilidi tam aktif (`Shift Up/Down` ve doğrudan `D/N/R`).
  - `Sıralı` (`shifter.type == "manual"`): `Shift Up` / `Shift Down` tuşları kilitsiz şekilde doğrudan oyuna (`gearup` / `geardown`) iletilir (böylece oyuncu mod değiştirince ETS2 içinde tekrar tuş atamak zorunda kalmaz); doğrudan `D/N/R` pasiftir.
  - `Basit Otomatik` (`"arcade"`) ve `H-Shifter` (`"hshifter"`): Eklenti tamamen pasif kalır.
- `D`, `N` ve `R` arasındaki **tüm** geçişlerde yalnızca **frene basılıyken** VE **araç tamamen durmuşken** vites değişimine izin vermek.
- Fren bilgisini doğrudan SCS Telemetry SDK üzerinden okuyarak klavye, gamepad ve tüm pedal setlerinde evrensel uyum sağlamak.
- Vites girişlerini eklenti içinde evrensel olarak dinlemek: klavye, XInput gamepad'ler (analog tetikler dahil) ve DirectInput/HID direksiyon setleri ile buton kutuları.
- İki vites kontrol biçimini aynı anda desteklemek (**anlık bas-bırak butonlar — Kullanıcı Kararı 2A**):
  1. **Sıralı kontrol:** `Shift Up` (`R → N → D`) ve `Shift Down` (`D → N → R`).
  2. **Doğrudan kontrol:** Ayrı `D` (Drive), `N` (Neutral), `R` (Reverse) anlık butonları (`D`ye basınca D'de kalır, bırakınca `N`'ye düşmez).
- Şart sağlanmadan (frene basılmadan veya araç hareket halindeyken) vites değiştirmeye çalışıldığında sesli uyarı vermek.
- İlk kurulumda tuş/buton atamasını, eşik ayarlarını ve ses seçimini kolaylaştıran hafif bir `.NET 9` yapılandırma aracı (`AutoShiftLockConfig.exe`) sunmak.

### Hedef Dışı Konular (Non-Goals)
- Araç `D` konumundayken ETS2'nin otomatik ileri vites geçiş oranlarına (`D1`..`D12`) müdahale etmek.
- Manuel, Sıralı veya H-Shifter şanzımanlar için debriyaj/vites kilidi yapmak.
- Fiziksel basılı tutmalı (latched) H-shifter yuvaları veya mekanik PRND vites kolları için konum-tutma (level-triggered) modu (v1 kapsamı dışı; v1 yalnızca anlık bas-bırak butonları destekler).
- Oyun içi DirectX/OpenGL görsel arayüz (HUD overlay) enjekte etmek (DLL stabilitesi ve multiplayer/anti-cheat uyumu için hariç tutulur).
- v1 kapsamında Linux/macOS desteği (Windows `x64` hedeflenir; aynı SCS SDK yapısını kullanan ATS uyumluluğu doğal yan fayda olarak korunur).

### Anti-Hedefler (Başarısızlık Kriterleri)
- ETS2 oynarken arka planda sürekli ikinci bir uygulamanın açık olmasını veya vJoy gibi sanal sürücü kurulumunu zorunlu kılmak.
- Eğimde veya dorse titremesinde ETS2 fiziğinin ürettiği `0.05 km/h` gibi mikro hız değerleri yüzünden vitesin kilitli kalması.
- Doğrudan `D` veya `R` tuşuna basıp bırakınca aracın tekrar `N`'ye düşmesi ya da çift vites atlaması.
- Kontrolcü oyun sırasında çıkarılıp takıldığında veya ses dosyası eksik olduğunda ETS2'nin çökmesi ya da takılması.
- Kullanıcı Alt-Tab ile başka pencerede yazı yazarken veya oyun duraklatılmışken tuş yakalayıp ses çalması/komut göndermesi.

## 3. Teknik Kısıtlar

1. **SCS Telemetry SDK salt okunurdur:** ETS2'nin kendi tuş atamalarını (örneğin oyunun doğrudan dinlediği `Shift Up` tuşunu) eklentiden bloke etmek mümkün değildir. Bu nedenle kullanıcı, ETS2 içindeki vites yükselt/düşür (veya otomatik ileri/geri) atamalarını boşaltır; fiziksel tuşlarını bizim eklentimize atar. Eklenti, şartlar sağlandığında SCS Input SDK üzerinden ETS2'ye sanal vites komutu iletir.
2. **ETS2 Gerçek Otomatik giriş davranışı:** ETS2'de `gearup` / `geardown` anlık darbe (pulse) ile `R ↔ N ↔ D` geçişi yapar; `geardrive` / `gearreverse` ise basılı tutma (latch) ister. Eklentimiz SCS Input SDK'nın `SCS_INPUT_DEVICE_TYPE_semantical` cihaz türünü kullanarak ETS2'ye semantik vites komutları gönderir; böylece kullanıcının ETS2 menüsünden ayrıca sanal tuş eşleştirmesi yapması gerekmez.
3. **Derleme araç zinciri:** Geliştirme makinesinde şu an `.NET 9 SDK` ve `winget` mevcuttur; native `x64` C++ DLL derlemek için kurulum aşamasında bir C++ derleyici (Visual Studio Build Tools veya MinGW-w64/LLVM) edinilecektir.

## 4. Değerlendirilen Yaklaşımlar ve Seçim

| Yaklaşım | Çalışma Şekli | Ödünler |
|---|---|---|
| **1. Native C++ ETS2 DLL + Küçük `.NET` Ayar Aracı *(Seçilen)*** | Tek `x64` DLL hem Telemetry hem Input SDK'yı kullanır; klavye/XInput/DirectInput dinler ve kilit sesini çalar. `.NET 9` aracı sadece ilk kurulumda tuş/ayar kaydetmek için açılıp kapatılır. | Native C++ derleyici gerektirir; ilk kurulumda ETS2'deki eski vites tuşlarının kaldırılması gerekir. |
| **2. İnce Köprü DLL + `.NET 9` Arka Plan Uygulaması** | İnce DLL veriyi shared memory ile dışarı aktarır; tüm mantık, cihaz okuma ve ses sistem tepsisi uygulamasında çalışır. | Oyun oynarken sürekli ikinci bir `.exe` açık kalmalıdır; yine de köprü DLL gerekir. |
| **3. Saf `.NET 9` + Hazır Telemetry DLL + `SendInput`** | Hazır telemetri eklentisinden okur; izin verince oyuna `F13`/`F14` gibi sanal klavye tuşu yollar. | Her kullanıcı ETS2'de sanal tuş atamak zorundadır; odak ve tam ekran sorunlarına açıktır. |

**Seçim:** Kullanıcı **1. yaklaşımı** seçti. Oyun sırasında sıfır arka plan uygulaması, sıfır harici sürücü ve gerçek ETS2 eklentisi deneyimi sunar.

## 5. Davranış ve Mimari Tasarım

### 5.1. Bileşenler
1. **`ets2_auto_shift_lock.dll` (C++17, x64):**
   - `win_x64/plugins/` klasörüne kurulur.
   - `scs_telemetry_init` / `scs_telemetry_shutdown`: Telemetri kanallarına (`truck.input.brake`, `truck.effective.brake`, `truck.speed`, `truck.gear`, `truck.displayed.gear`), konfigürasyona (`shifter.type`) ve yaşam döngüsü olaylarına (`paused`, `started`, `frame_end`) abone olur.
   - `scs_input_init` / `scs_input_shutdown`: `SCS_INPUT_DEVICE_TYPE_semantical` türünde sanal cihaz kaydederek ETS2'ye vites komutlarını (`gearup`, `geardown`) iletir.
   - **Giriş Dinleyici Modülü (Yükselen Kenar / Edge-Triggered):**
     - **Klavye:** Win32 `GetAsyncKeyState` / Raw Input (sadece ETS2 ön plandayken ve `paused == false` iken).
     - **Gamepad:** XInput (`XInputGetState`, 0–3 arası tüm slotlar; dijital butonlar + `LT`/`RT` analog tetikleri `%50` eşikle dijital buton gibi desteklenir).
     - **Direksiyon / Buton Kutusu:** DirectInput8 (`IDirectInput8` ile bağlı tüm joystick/wheel/button-box cihazlarının buton ve POV yönleri; cihaz eşleştirmesi Product GUID + cihaz adı ile yapılır, USB portu değişse de çalışır; periyodik yeniden tarama ile tak-çıkar dayanıklıdır).
     - Tüm vites eylemleri **yükselen kenar (press edge)** ile tetiklenir; tuşa basılı tutmak tekrarlı tetikleme yapmaz ve bırakınca vites boşa düşmez.
   - **Vites Kilidi Durum Makinesi (Saf C++ modül, birim test edilebilir):**
     - **Mod yönlendirmesi:**
       - `shifter.type == "arcade"` veya `"hshifter"` (ya da bilinmeyen): tüm girişler yok sayılır.
       - `shifter.type == "manual"` (ETS2 Sıralı modu): `Shift Up` → doğrudan `gearup` darbesi, `Shift Down` → doğrudan `geardown` darbesi (fren/hız kontrolü yok, uyarı sesi yok); `D`/`N`/`R` yok sayılır.
       - `shifter.type == "automatic"` (ETS2 Gerçek Otomatik modu): aşağıdaki vites kilidi kuralları uygulanır.
     - Mevcut aralığı telemetriden türetir:
       - `displayed_gear < 0` (veya `gear < 0`) → `R (-1)`
       - `displayed_gear == 0` (ve `gear == 0`) → `N (0)`
       - `displayed_gear > 0` (veya `gear > 0`) → `D (+1)`
     - İstenen hedef aralığı belirler:
       - `Shift Up`: `R → N`, `N → D`, `D → no-op`
       - `Shift Down`: `D → N`, `N → R`, `R → no-op`
       - `Direct D / N / R`: doğrudan hedef aralık (`+1`, `0`, `-1`); zaten aynı aralıktaysa `no-op`.
     - **Kilit Kontrolü (Kullanıcı Kararı):**
       - Hedef aralık mevcut aralıktan farklıysa:
         - `brake_value >= brake_threshold` **VE** `abs(speed_kmh) <= stop_speed_kmh` kontrol edilir.
         - **Koşul sağlanırsa:** Hedefe ulaşmak için gereken `gearup` veya `geardown` darbe(ler)i kuyruğa alınır (örneğin `D → R` için `geardown`, kısa bekleme, tekrar `geardown`; her adımda telemetri güncelliği gözetilir).
         - **Koşul sağlanmazsa:** Hiçbir vites darbesi gönderilmez ve sesli uyarı tetiklenir.
     - **Aynı aralıkta tekrar basma (Tasarım önerisi):** Zaten `D` iken `D` veya `Shift Up`'a basılırsa uyarı sesi çalınmaz ve komut gönderilmez (sessiz no-op).
   - **Sesli Uyarı Modülü:**
     - Win32 `PlaySoundW(..., SND_ASYNC | SND_NODEFAULT)` ile eklenti yanındaki `lock_warning.wav` dosyasını oyun döngüsünü bloklamadan çalar.
     - Dosya bulunamazsa gömülü/sentetik kısa uyarı tonuna veya `MessageBeep`'e güvenli geri dönüş yapar.
     - Üst üste basışlarda rahatsızlık vermemek için bekleme süresi (`sound_cooldown_ms`, varsayılan `350 ms`) uygular.

2. **`ets2_auto_shift_lock.ini` (Eklenti ile aynı klasörde):**
   - İnsan tarafından okunabilir, hem DLL hem de Ayar Aracı tarafından ortak kullanılan yapılandırma dosyası.
   - Örnek anahtarlar:
     - `[thresholds]`: `brake_threshold=0.25`, `stop_speed_kmh=0.5`, `brake_source=input` (`input` = sürücü ayak freni pedalı, `effective` = efektif fren)
     - `[audio]`: `enabled=true`, `sound_file=lock_warning.wav`, `cooldown_ms=350`
     - `[timing]`: `pulse_ms=60`, `pulse_gap_ms=80`
     - `[bindings]`: `shift_up=...`, `shift_down=...`, `gear_d=...`, `gear_n=...`, `gear_r=...` (boş bırakılanlar pasif; örn. `kb:160`, `xinput:0:RB`, `dinput:{GUID}:btn:4`)

3. **`AutoShiftLockConfig.exe` (C# / .NET 9 Windows Masaüstü Uygulaması):**
   - Yalnızca kurulumda veya ayar değiştirmek istendiğinde açılır; oyun sırasında çalışması gerekmez.
   - Özellikler:
     - ETS2 `win_x64/plugins` yolunu otomatik algılama (veya elle seçme) ve tek tıkla DLL + varsayılan `.wav` kopyalama / güncelleme.
     - Her vites eylemi (`Shift Up`, `Shift Down`, `D`, `N`, `R`) için **"Tuşa / Butona Basarak Ata"** (klavye, XInput ve DirectInput cihazlarını DLL ile birebir aynı formatta yakalar) ve "Temizle" düğmesi.
     - Fren eşiği (%), durma hızı toleransı (`km/h`), uyarı sesi aç/kapat, özel `.wav` seçme ve "Sesi Test Et" düğmesi.
     - Kısa kurulum rehberi: *"ETS2 Ayarlar → Tuşlar ve Düğmeler bölümünden mevcut Vites Yükselt / Düşür atamalarını kaldırın; vites tipini 'Gerçek Otomatik' seçin."*

### 5.2. Bizim Önerdiğimiz Varsayılan Kararlar (Kullanıcı Kontrolü İçin)
- **Durma toleransı:** Varsayılan `0.5 km/h` (ETS2 fizik titremesini absorbe eder, `.ini` ve Ayar Aracı'ndan değiştirilebilir).
- **Fren eşiği ve kaynağı:** Varsayılan `%25` (`0.25`) ve `truck.input.brake` (sadece sürücü ayak freni; el freni/park freni vites kilidini açmaz — gerçek araçlardaki gibi).
- **Aynı vitese tekrar basma:** Zaten `D` konumundayken `D` veya `Shift Up` tuşuna basıldığında sessizce yok sayılır (uyarı sesi çalmaz).
- **Tuş atama modeli (v1):** Her eylem (`shift_up`, `shift_down`, `gear_d`, `gear_n`, `gear_r`) için **tek bir tuş/buton ataması** yapılır (karmaşık kombinasyon/çoklu slot yok); gamepad kullanıcıları için XInput analog tetikleri (`LT`/`RT`, `%50` eşikle) tek buton gibi atanabilir.
- **Paketleme ve dağıtım:** Son kullanıcı dağıtımı tek bir ZIP paketi olarak hazırlanır: içinde bağımsız (self-contained, ek `.NET Desktop Runtime` kurulumu gerektirmeyen) `AutoShiftLockConfig.exe` ve yanında `plugin/` klasöründe `ets2_auto_shift_lock.dll` + `lock_warning.wav` bulunur. Ayar Aracı "Kur / Güncelle" düğmesiyle `plugin/` içeriğini ETS2 `win_x64/plugins/` klasörüne kopyalar.
- **Konfigürasyon güncelleme:** Oyun kapatılmadan yapılan `.ini` değişiklikleri (örneğin Alt-Tab ile Ayar Aracı'ndan tuş değiştirildiğinde) dosya değişiklik zamanı kontrol edilerek otomatik yeniden yüklenir.

## 6. Uygulayıcıya (Builder) Bırakılan Detaylar
- CMake proje yapısı, SCS SDK 1.14 başlık dosyalarının entegrasyonu ve exact `scssdk_input.h` semantik isim doğrulaması (`gearup`/`geardown` vs `drive`/`reverse` ETS2 sürüm testi).
- DirectInput cihazlarının ayrı polling thread'de mi yoksa `frame_end` tick içinde mi okunacağı (oyun FPS'ini etkilemeyecek şekilde).
- Varsayılan `lock_warning.wav` ses dosyasının üretimi (temiz mekanik shift-lock tık / kısa uyarı tonu).
- Saf C++ shift-lock durum makinesi için birim test paketi (fren altı/üstü, hız altı/üstü, D↔N↔R tüm kombinasyonlar, çift darbe sıralaması, cooldown, mod geçişi).
- `AutoShiftLockConfig.exe` için WPF veya WinForms arayüz düzeni ve görsel tasarımı.
