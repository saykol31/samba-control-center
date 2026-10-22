# Samba Control Center

Aplicación de escritorio (GTKmm 4 / C++17) para montar y administrar un
**servidor Samba** en tu propio equipo Linux: comparte una carpeta con tu
red local (por ejemplo, con un equipo Windows) sin tocar archivos de
configuración a mano.

Todo se opera desde una sola ventana: panel de estado en vivo, botones de
acción y un registro (log) que muestra, paso a paso, lo que hizo cada
acción y con qué resultado.

- Carpeta compartida: `~/Red`, publicada como el recurso **`[compartida]`**.
- Configuración generada: `/etc/samba/smb.conf` (workgroup `WORKGROUP`,
  seguridad por usuario: `security = user`).
- Probado en MX Linux (SysVinit). El gestor de servicios se detecta en
  tiempo de ejecución (`/proc/1/comm`): systemd usa `systemctl`,
  SysVinit/OpenRC usa `service`, con respaldo a `/etc/init.d/`.

## Requisitos

Sistema Debian o derivado, con entorno de escritorio.

## Compilar

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libgtkmm-4.0-dev
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Ejecutar

```bash
./build/samba-control-center
```

La aplicación arranca **sin privilegios**: lee tu usuario y tu carpeta
personal, y solo pide autorización de administrador cuando una acción lo
necesita.

## Cómo funciona

### La ventana

- **Panel de estado**: servicios `smbd`/`nmbd`, firewall UFW, puertos
  139/445 y estado de red. El estado de UFW arranca en "SIN VERIFICAR":
  abrir la aplicación no pide ninguna contraseña.
- **Registro**: cada acción escribe su resultado en el log. Si algo
  falla, el log dice qué paso falló y por qué; ninguna acción reporta
  éxito si un paso intermedio falló.

### Los botones

| Botón | Qué hace |
|---|---|
| **DEPENDENCIAS** | Comprueba que estén `smbd`, `smbclient`, `smbpasswd`, `testparm`, `tcpdump`, `ufw`, `ss`, `ping` y `nc`. Si falta algo, pregunta **una sola vez** si quieres instalarlo todo. |
| **CONFIGURAR SAMBA** | El flujo completo, en orden: crea la carpeta `~/Red` → valida la nueva configuración con `testparm` **antes** de instalarla → respalda el `smb.conf` actual → instala el nuevo → comprueba si tu usuario Samba existe → reinicia `smbd` y `nmbd` → abre el firewall. |
| **CONTRASEÑA SAMBA** | Crea o cambia la contraseña Samba de tu usuario (la que usarás desde Windows para entrar). |
| **REINICIAR SERVICIOS** | Reinicia `smbd` y `nmbd`. |
| **FIREWALL UFW** | Permite el perfil Samba en UFW (`ufw allow samba`). |
| **PUERTOS 139/445** | Muestra qué procesos escuchan en los puertos de Samba. |
| **RED / RUTAS** | Interfaces IPv4, tabla de rutas, puerta de enlace y vecinos ARP. |
| **PING WINDOWS / PRUEBA SMB** | Ping al equipo Windows y prueba de conexión SMB/TCP 445 contra él. |
| **TCPDUMP** | Captura breve de tráfico para depurar la conexión con Windows. |
| **DIAGNÓSTICO COMPLETO** | El informe entero de una vez: dependencias, versión de Samba, carpeta, servicios, UFW, puertos, red, validación de la configuración (`testparm`), usuario Samba, procesos y sockets. |
| **LIMPIAR** | Vacía el registro. |

### Autorización única: la contraseña se pide una sola vez

Las acciones que necesitan root no piden la contraseña comando a
comando. Cada acción encadena todos sus pasos con privilegios en **un
solo script** ejecutado con **un único `pkexec`**:

- Autorizas **una vez** por acción, aunque la acción tenga ocho pasos.
- Si un paso falla, los pasos siguientes **se saltan**: nada queda a
  medias sin que el log lo diga.
- Las consultas de solo lectura (`ufw status`, `pdbedit -L`) viajan
  dentro de ese mismo script, también en el diagnóstico.
- Si cancelas la autorización, la aplicación lo dice tal cual
  ("no se pudo verificar"), en lugar de afirmar que algo no existe.

Esto está verificado en la práctica: Configurar Samba y Diagnóstico
Completo piden la contraseña **exactamente una vez** cada uno.

### Respaldo de la configuración

Antes de instalar un `smb.conf` nuevo, el actual se respalda con un
nombre único con fecha y hora, sin sobrescribir respaldos anteriores:

```
/etc/samba/smb.conf.backup-AAAAMMDD-HHMMSS-<pid>
```

La configuración nueva se valida con `testparm` sobre el archivo
temporal **antes** de tocar el `smb.conf` en uso: una configuración
inválida nunca llega a instalarse.

### Apuntar los diagnósticos a tu equipo Windows

Al arrancar, la aplicación crea (si no existe) el archivo:

```
~/.config/samba-control-center/config
```

Edítalo y pon la IP de tu equipo Windows:

```
windows_ip=192.168.1.50
```

La IP se valida al cargarla. Sin una IP válida, los botones dirigidos a
Windows (ping, prueba SMB, tcpdump) muestran un aviso informativo y el
resto de la aplicación funciona con normalidad.

### Conectarte desde Windows

Con el servidor configurado y tu contraseña Samba creada, desde el
Explorador de archivos de Windows:

```
\\<IP-de-tu-Linux>\compartida
```

Inicia sesión con tu usuario Linux y tu contraseña Samba.

## Estructura del proyecto

```
CMakeLists.txt
src/main.cpp                                  La aplicación completa
resources/icons/hicolor/256x256/apps/         Icono
```
