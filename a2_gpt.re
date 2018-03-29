= GPTフォーマット方法
UEFIファームウェアは、起動用のストレージ等のGPTあるいはFATフォーマットされた第1パーティションを認識します。そのため、GPTあるいはFATでフォーマットしていないとUEFIファームウェアが"EFI/BOOT/BOOTX64.EFI"という名前で配置したブートローダー(poiboot)を見つけてくれません。

ここでは旧来のMS-DOSパーティションテーブルではなく、UEFIで新仕様として加わったGPT(GUID Partition Table)形式でUSBフラッシュメモリをフォーマットしてみます。

GPT形式のフォーマットに、ここではgdiskコマンド(fdiskのGPT対応版)とmkfsを使用します。

まずはgdiskでGPTのパーティションテーブルを作成します(USBフラッシュメモリ第1パーティションが"/dev/sdb1"として認識されているとします)。

//cmd{
$ @<b>{sudo gdisk /dev/sdb}
GPT fdisk (gdisk) version 0.8.10

Partition table scan:
  MBR: protective
  BSD: not present
  APM: not present
  GPT: present

Found valid GPT with protective MBR; using GPT.

Command (? for help): @<b>{o}  ← GPTを作成する指定
This option deletes all partitions and creates a new protective MBR.
Proceed? (Y/N): @<b>{Y}

Command (? for help): @<b>{n}
Partition number (1-128, default 1):
First sector (34-31277022, default = 2048) or {+-}size{KMGTP}:
Last sector (2048-31277022, default = 31277022) or {+-}size{KMGTP}:
Current type is 'Linux filesystem'
Hex code or GUID (L to show codes, Enter = 8300): @<b>{ef00}
Changed type of partition to 'EFI System'

Command (? for help): @<b>{p}
Disk /dev/sdb: 31277056 sectors, 14.9 GiB
Logical sector size: 512 bytes
Disk identifier (GUID): 2DDA5A7F-F45A-4B9C-9AB1-137FC2856D83
Partition table holds up to 128 entries
First usable sector is 34, last usable sector is 31277022
Partitions will be aligned on 2048-sector boundaries
Total free space is 2014 sectors (1007.0 KiB)

Number  Start (sector)    End (sector)  Size       Code  Name
   1            2048        31277022   14.9 GiB    EF00  EFI System

Command (? for help): @<b>{w}

Final checks complete. About to write GPT data. THIS WILL OVERWRITE EXISTING
PARTITIONS!!

Do you want to proceed? (Y/N): @<b>{Y}
OK; writing new GUID partition table (GPT) to /dev/sdb.
The operation has completed successfully.
//}

"o"コマンドでGPTを作成する指定をし、USBフラッシュメモリの全容量を使用する単一のパーティションを作成しています。"ef00"は"EFI System Partition"というUEFI起動ディスク専用のパーティションタイプです。

次に、作成したパーティションをFAT32でフォーマットします。

//cmd{
$ @<b>{sudo mkfs.vfat -F 32 /dev/sdb1}
mkfs.fat 3.0.27 (2014-11-12)
//}
