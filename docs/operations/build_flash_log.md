# Build and Flash Log

## Entry template

### Date

### Branch

### Commit

### Environment

- OS:
- WSL:
- Ubuntu:
- ESP-IDF: v5.1.3
- Python:
- VS Code:
- Device:

### Commands

```bash
python ./fetch_repos.py
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
idf.py -p <YourPort> flash -b 1500000
```

### Result

- dependency fetch:
- build:
- flash:
- boot:

### Failure / notes

### Unverified

- 
