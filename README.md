# samsung-laptop-tools
<center><img width="1556" height="1050" alt="Image" src="https://github.com/user-attachments/assets/7d4be503-0bc6-49cd-9774-5157afec47f8" /></center>
easy-peasy battery health &amp; intel turbo boost control tools

## Changelog
#### Changelog samsung-laptop-tools v0.2:
- fix GTK widget works incorrectly
- add codes to prevent memory leaks by saving type data
- add basic validation steps for security

## Requirements
- Build tools(such as pkg-config), GTK-4-dev, pkexec for applying values to proper sysfs file.

## Installation
- No other steps are needed, just compile main.c with 
```
gcc `pkg-config --cflags gtk4` -o samsung-laptop-tools main.c `pkg-config --libs gtk4`
```

## Comments

- It's based on NT930QCA(NP930QCA for global), Intel 11th gen i7-1165G7, Ubuntu_26.04_AMD64
- It will find battery_life_extender, charge_control_end_threshold, and no_turbo files in /sys automatically. Otherwise, you can choose the proper sysfs file path.
