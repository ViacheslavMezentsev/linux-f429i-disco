# BusyBox top для STM32F429I-DISCO

В WSL из каталога пакета выполните:

```sh
sha256sum -c SHA256SUMS
tar -xzf busybox-top-1.36.1-armv7m-bflt.tar.gz top
scp -p -P 2222 top user@192.168.68.73:/tmp/top
ssh -t -p 2222 user@192.168.68.73 '/tmp/top -d 2'
```

В OpenSSH с передачей по SFTP по умолчанию добавьте к scp параметр `-O`. Сохраните имя `top` и право исполнения. Выход из монитора — клавиша `q`. Файлы в /tmp находятся в SDRAM.

Проверен на Linux 6.14.5. Сопутствующие архивы содержат исходники BusyBox и uClibc-ng с лицензиями. Для пересборки используйте BusyBox из приложенного архива, checkout uClibc-ng указанного в manifest.txt коммита, busybox.config и Bootlin SDK из docs/SOURCES.md. Команда из корня проекта:

```sh
sh tools/build-lowmem-busybox.sh "$UCLIBC_GIT" "$BUSYBOX_SRC" \
    artifacts/armv7m-bflt/busybox-top/1.36.1-r1/busybox.config "$SDK" "$NEW_OUTPUT"
```

Время компиляции включено в баннер BusyBox. SHA-256 в манифесте относится к бинарнику, проверенному на плате.
