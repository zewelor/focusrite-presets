# Focusrite Scarlett Routing Presets

Kolekcja profilów i plików stanu dla interfejsów **Focusrite Scarlett** pod Linuksem (używanych np. z `alsa-scarlett-gui` / `scarlett2-gui` lub mikserem ALSA kernel driver).

## 📁 Struktura katalogów

```text
focusrite-presets/
├── presets/             # Pliki profili / ustawień routingowych (.json, .state, .alsa)
│   ├── default.json
│   └── ...
└── README.md
```

## 🛠️ Jak używać (alsa-scarlett-gui / scarlett2-gui)

1. **Zapis stanu:**
   W programie `alsa-scarlett-gui` / `scarlett2-gui` zapisz swój aktualny stan/routing do pliku w folderze `presets/`.

2. **Wczytanie stanu:**
   Otwórz aplikację GUI i załaduj wybrany plik stanu z tego repozytorium.

---

### English Summary
Public repository storing Focusrite Scarlett audio interface routing presets and state files for Linux (`alsa-scarlett-gui`).
