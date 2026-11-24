# ETS2 Otomatik Vites Fren Kilidi Eklentisi (Auto Shift-Lock)

Euro Truck Simulator 2 (ETS2) **"Gerçek Otomatik"** (`Real Automatic`) şanzıman modunda, gerçek otomatik vitesli araçlardaki gibi **`D`, `N` ve `R` arasında yalnızca frene basılıyken ve araç tamamen durmuşken** vites değiştirmenize izin veren yerel (native `x64`) ETS2 eklentisi.

- **Evrensel donanım desteği:** Klavye, Xbox/XInput gamepad (tetikler dahil) ve tüm DirectInput/HID direksiyon setleri ile buton kutuları.
- **Sıfır arka plan uygulaması & sıfır sanal sürücü:** Oyun sırasında yalnızca ETS2'nin yüklediği `ets2_auto_shift_lock.dll` çalışır (vJoy gerekmez).
- **Çift vites kontrolü:** Hem sıralı `Vites Yükselt / Düşür` (`R ↔ N ↔ D`) hem de ayrı anlık `D`, `N`, `R` butonlarını destekler.
- **Akıllı mod algılama:**
  - **Gerçek Otomatik:** Fren + tam durma kilidi aktif.
  - **Sıralı (Sequential):** `Vites Yükselt / Düşür` tuşları kilitsiz olarak oyuna iletilir (mod değiştirince yeniden tuş atamanız gerekmez).
  - **Basit Otomatik / H-Shifter:** Eklenti otomatik olarak pasif kalır.
- **Sesli uyarı:** Frene basmadan veya araç hareket halindeyken vites değiştirmeye çalışırsanız kilit uyarı sesi çalar.

## Kurulum (Son Kullanıcılar)

1. ZIP arşivini bir klasöre çıkarın ve `AutoShiftLockConfig.exe` uygulamasını açın.
2. ETS2 `bin\win_x64\plugins` klasörü otomatik algılanır (algılanmazsa **Gözat...** ile seçin).
3. **Eklentiyi Kur / Güncelle** butonuna basın (`ets2_auto_shift_lock.dll` ve `lock_warning.wav` kopyalanır).
4. Kullanmak istediğiniz vites tuşlarını (**Vites Yükselt / Düşür** ve/veya **Doğrudan D / N / R**) **Tuşa Basarak Ata** ile tanımlayıp **Ayarları Kaydet** butonuna basın.
5. **ETS2 içinde tek seferlik ayar:**
   - **Ayarlar → Kontroller → Şanzıman:** `Gerçek Otomatik` seçin.
   - **Ayarlar → Tuşlar ve Düğmeler:** ETS2'nin mevcut **Vites Yükselt** ve **Vites Düşür** atamalarını kaldırın (böylece oyun frensiz vites değiştirmez; geçişleri bizim eklentimiz kontrol eder).
6. ETS2'yi açtığınızda SDK uyarısını onaylayın; oyun oynarken `AutoShiftLockConfig.exe`'nin açık kalmasına gerek yoktur.

## Ayar Dosyası (`ets2_auto_shift_lock.ini`)

Ayarlar `win_x64\plugins\ets2_auto_shift_lock.ini` dosyasında saklanır ve oyun açıkken değiştirilirse otomatik yeniden yüklenir:

```ini
[thresholds]
brake_threshold=0.25
stop_speed_kmh=0.50
brake_source=input

[audio]
enabled=true
sound_file=lock_warning.wav
cooldown_ms=350

[timing]
pulse_ms=60
pulse_gap_ms=80

[bindings]
shift_up=kb:160
shift_down=kb:162
gear_d=
gear_n=
gear_r=
```

## Kaynaktan Derleme

Gereksinimler: Windows x64, MinGW-w64 (UCRT x64) veya MSVC + CMake + `.NET 9 SDK`.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1
```

Çıktı paketi `dist/ets2-auto-shift-lock-v1.0.0-win-x64.zip` altında oluşturulur.
