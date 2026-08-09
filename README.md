# Focusrite Scarlett Routing Presets

Kolekcja profilów i plików stanu dla interfejsów **Focusrite Scarlett** pod Linuksem (używanych np. z `alsa-scarlett-gui` / `scarlett2-gui` lub mikserem ALSA kernel driver).

## 📁 Struktura katalogów

Każdy preset znajduje się we własnym podkatalogu wewnątrz `presets/` i zawiera plik stanu (`.state`) oraz plik `README.md` z opisem połączeń.

```text
focusrite/
├── presets/
│   ├── tv_rpi/
│   │   ├── tv_rpi.state      # Plik stanu ALSA mixer
│   │   └── README.md         # Opis i schemat połączeń presetu
│   └── ...
└── README.md
```

## 📋 Lista dostępnych presetów

| Preset | Opis | Link |
| ------ | ---- | ---- |
| **tv_rpi** | Routing z TV (Analogue 3/4) + RPi (PCM 1/2) do KRK (Output 1/2) oraz Słuchawek (Output 5/6). | [`presets/tv_rpi/`](./presets/tv_rpi/) |

## 🛠️ Jak używać (alsa-scarlett-gui / scarlett2-gui)

1. **Zapis nowego presetu:**
   Stwórz nowy podkatalog w `presets/<nazwa_presetu>/`, zapisz tam plik `.state` oraz utwórz `README.md` ze schematem połączeń.

2. **Wczytanie stanu:**
   Wczytaj plik `.state` z wybranego podkatalogu w aplikacji `alsa-scarlett-gui` / `scarlett2-gui` lub przy użyciu `alsactl restore -f <plik.state>`.

## 🔧 Narzędzia sprzętowe

W katalogu [`scarlett-front-panel/`](./scarlett-front-panel/) znajduje się jednorazowy helper USB dla **Scarlett 4i4 4th Gen z firmware 2417+**. Pozwala odczytać i ustawić:

- jasność panelu: `high`, `medium`, `low`;
- automatyczne wygaszanie po bezczynności: `0–86400` sekund (`0` oznacza brak wygaszania).

Helper działa niskopoziomowo przez libusb, więc nie wymaga rekompilacji kernela. Przed uruchomieniem przeczytaj osobny [`README.md`](./scarlett-front-panel/README.md); wrapper `run-exclusive` zatrzymuje WirePlumbera i tymczasowo odłącza `snd_usb_audio`, aby uniknąć konfliktu z kernelem.

Przykład konfiguracji najciemniejszego panelu i wygaszania po pięciu minutach:

```sh
cd scarlett-front-panel
make
./run-exclusive --brightness low --sleep 300
```

---

### English Summary
Public repository storing Focusrite Scarlett audio interface routing presets and state files for Linux (`alsa-scarlett-gui`). Each preset is stored in its own folder under `presets/` along with a `README.md` describing its signal flow.
