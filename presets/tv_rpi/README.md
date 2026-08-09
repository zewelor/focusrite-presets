# Preset: TV + RPi → Scarlett → KRK (`tv_rpi`)

Konfiguracja routingu audio dla Focusrite Scarlett łącząca wejścia z telewizora (TV) oraz Raspberry Pi (RPi) i kierująca je do głośników KRK oraz słuchawek.

## 🎵 Schemat połączeń (Routing Diagram)

```text
TV L  → Analogue Input 3 ─┐
                          ├→ Mix A → Analogue Output 1 (KRK L) & Output 5 (Headphones L)
RPi L → PCM Playback 1 ───┘

TV R  → Analogue Input 4 ─┐
                          ├→ Mix B → Analogue Output 2 (KRK R) & Output 6 (Headphones R)
RPi R → PCM Playback 2 ───┘
```

## ⚙️ Wyjaśnienie konfiguracji

### Wyjścia (Outputs)
* **Analogue Output 1:** Mix A (Główny lewy kanał KRK)
* **Analogue Output 2:** Mix B (Główny prawy kanał KRK)
* **Analogue Output 5:** Mix A (Słuchawki / dodatkowe wyjście L)
* **Analogue Output 6:** Mix B (Słuchawki / dodatkowe wyjście R)

### Wejścia i Poziomy Głośności (Mixer Inputs & Volume)
* **Mix A (Kanał Lewy):**
  * `Input 3 / Analogue 3` = **0 dB** (TV L)
  * `Input 5 / PCM 1`      = **0 dB** (RPi L)
  * Pozostałe wejścia       = **-∞**
* **Mix B (Kanał Prawy):**
  * `Input 4 / Analogue 4` = **0 dB** (TV R)
  * `Input 6 / PCM 2`      = **0 dB** (RPi R)
  * Pozostałe wejścia       = **-∞**

### Głośność Główna
* `Master HW Playback Volume` ustawiony na około **-6 dB** (dodatkowy zapas bezpieczeństwa dla KRK).

## 📂 Plik stanu
- Plik stanu ALSA: [`tv_rpi.state`](./tv_rpi.state)
