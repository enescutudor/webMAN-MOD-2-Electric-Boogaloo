# webMAN-MOD: fork

Fork al [aldostools/webMAN-MOD](https://github.com/aldostools/webMAN-MOD), pornit de la `70ad8ef`. Ținta: optimizare (mod dual, economic în joc și complet în XMB), apoi securitate.

## Regula de bază: zona low-level nu se atinge

Fișierele care lucrează cu flash-ul, EPROM-ul, LV1/LV2, syscall-urile Cobra, registrul sau ventilatorul **nu se modifică** în fork. Se preiau neschimbate din upstream.

- `PROTECTED_PATHS.txt`: zona roșie. Un commit care o atinge e refuzat.
- `tools/amber-paths.txt`: zona galbenă. Commit-ul trece doar dacă mesajul are secțiunea `Impact low-level`, care spune ce se schimbă și de ce e sigur.

Check-ul:

```sh
git config core.hooksPath tools/hooks   # o dată, după clonare: activează pre-commit și commit-msg
tools/check-protected.sh                 # tot fork-ul față de upstream/master (exit 1 dacă e atinsă zona roșie)
```

Merge-urile din upstream trec. Ele aduc modificările upstream neschimbate.

## Build

Cu Cell SDK (`CELL_SDK` setat, `ppu-lv2-*` în `PATH`) și `scetool` în `PATH`: `./_Make.sh`. Scriptul se oprește dacă `make` eșuează.
