/ Загрузчик с RK05       (СМ 5400),
/             RP03       (ЕС 5061),
/             RK06/1  (СМ 5408),
/             RP04/05/06,
/             RL01/02,
/             RM02/03/05,
/             Diva Comp. IV controller (33 сектора)  (DVHP)
/             RD 50/51          (Электроника-85)
/             флоппи Э-85
/             флоппи ДВК-4 my
/
/  $Log:	uboot.s,v $
/ Revision 1.7  90/12/12  17:09:00  korotaev
/ Вставлены my и dw (от rw) для ДВК.
/ 
/ Revision 1.6  89/06/01  15:31:52  avg
/ Вставлена правка под новую fs.
/ 
/ Revision 1.5  88/10/02  19:37:09  avg
/ Сделана загрузка с ненулевого драйва на RP и HK.
/
/ Revision 1.4  88/01/15  14:07:20  korotaev
/ Изменения связаны с внесением rauboot, причем отдельным файлом (!!!).
/ Так сделали в АЗЛК.
/
/ Revision 1.3  88/01/05  16:28:58  korotaev
/ Состояние перед слиянием ядер ИПК и АЗЛК.
/
/ Revision 1.2  86/07/13  14:06:02  avg
/ Добавлен загрузчик с Винчестера  Электроники-85.
/
/ Этот загрузчик должен записываться в блок #1 !!!
/
/ Revision 1.1  86/07/10  22:46:09  avg
/ Initial revision
/
/
/ Для использования блоков по 1 КБ надо установить CLSIZE в 2.
/ Число прямых адресов I-узла - NDIRIN (сейчас 4)
/ Размер должен быть <= 512 байтов; если > 494, то полезно обрезать
/ 16-битовый заголовок a.out
/

/ ------ Макроопределения: -----------------------------------------------








major=4.













/ ------ Флаги: ------------------------------------------------------------
nohead  = 1             / 0-> нормальный, 1-> этот загрузчик не должен
			/ содержать заголовка a.out. ( -10 байтов )
readname= 0             / 1-> нормальный, если файл по умолчанию не найден,
			/ читать имя с консоли, 0-> зациклиться ( -36 байтов )
prompt  = 1             / 1-> приглашение ('>') перед чтением с консоли
			/ 0-> нет приглашения. ( -8 байтов )
autoboot= 1             / 1-> код для автозагрузки.
			/ 0-> нет автозагрузки, ( -12 байтов )
cyl     = 0.            / номер первого цилиндра файловой системы
			/ (rp,hk,hp,rm,dvhp)
drive   = 0             / устройство для загрузки (hp,rm,dvhp)

xautoboot=1             / авт. загрузка не с 0-го привода





   rk07 = 1             / 1-> 1, 0-> RK06





/ ------ Kонстанты: --------------------------------------------------------
CLSIZE  = 2.                    / физ. блоков диска на один логический

CLSHFT  = 1.                    / сдвиг для умножения на CLSIZE
BSIZE   = 512.*CLSIZE           / логический размер блока
INOSIZ  = 64.                   / размер I-узла в байтах
NDIRIN  = 4.                    / число прямых адресов I-узла
ADDROFF = 12.                   / смещение первого адреса в I-узле
INOPB   = BSIZE\/INOSIZ         / число I-узлов в логическом блоке
INOFF   = 31.                   / смещение I-узлов = (INOPB * (SUPERB+1)) - 1
WC      = -256.*CLSIZE          / счетчик слов

/  Режим загрузки и устройство заносятся ядром в последние
/  SZFLAGS байтов памяти при автозагрузке.
ENDCORE=        160000          / конец памяти при выключенном ДП
SZFLAGS=        8.              / размер флагов загрузчика
BOOTOPTS=       2               / расположение флагов, байтов перед ENDCORE
BOOTDEV=        4
CHECKWORD=      6
RBOOTDEV=       8.



.. = ENDCORE-512.-SZFLAGS       / Место для флагов загрузчика


/ Определения для FD (флоппи Э85)


start:



/ управление передается по jsr pc,*$0
/ так что можно возвратить по rts pc

/ установить sp, скопировать
/ программу к концу памяти.

	mov     $..,sp



	mov     sp,r1

.if xautoboot        / В регистре 0 остается номер драйва от hardboot
	mov     r0,r5
.endif

	clr     r0
.if     nohead-1                        / Если nohead == 1
	cmp     (r0),$407
	bne     1f
	mov     $20,r0
.endif
1:
	mov     (r0)+,(r1)+
	cmp     r1,$end
	blo     1b
	jmp     *$2f


/ При ошибке перезапускается отсюда.
restart:

/ Очистить память
	clr     r0
2:

	mov     $0,(r0)+

	cmp     r0,sp
	blo     2b

/
/ Проверить слова для определения номера у-ва загрузки
/
.if xautoboot
	mov     ENDCORE-BOOTOPTS,r0
	com     r0
	cmp     ENDCORE-CHECKWORD,r0
	beq     9f
/
/ Загрузка аппаратным загрузчиком. Попробуем определить накопитель
/

	ash     $3,r5
	bic     $!70,r5



	mov     r5,ENDCORE-BOOTDEV

9:
.endif

/ инициализировать диск








	mov     $clear,*$hkcs2          / сброс подсистемы
	mov     $ack,*$hkcs1
0:
	tstb    *$hkcs1
	bpl     0b                      / ждать завершения операции







/ читать имя файла

.if     prompt
	mov     $'>, r0
	jsr     pc, putc
.endif


/ запоминается в массиве 'names', каждый компононет
/ пути в каждых 14 байтах.
	mov     $names,r1
1:
	mov     r1,r2
2:
	jsr     pc,getc

	cmp     r0,$'\n

	beq     1f
.if     readname
	cmp     r0,$'/
	beq     3f
.endif
	movb    r0,(r2)+
	br      2b
.if     readname
3:
	cmp     r1,r2
	beq     2b
	add     $14.,r1
	br      1b
.endif

/ запустить чтение I-узлов
/ с корневого каталога и далее по каталогам
1:
	mov     $names,r1
	mov     $2,r0
1:
	clr     bno
	jsr     pc,iget
	tst     (r1)
	beq     1f
2:
	jsr     pc,rmblk
		br restart
	mov     $buf,r2
3:
	mov     r1,r3
	mov     r2,r4
	add     $16.,r2
	tst     (r4)+
	beq     5f
4:
	cmpb    (r3)+,(r4)+
	bne     5f
	cmp     r4,r2
	blo     4b
	mov     -16.(r2),r0
	add     $14.,r1
	br      1b
5:
	cmp     r2,$buf+BSIZE
	blo     3b
	br      2b

/ читать файл в память до первой ошибки
/ ("нет такого блока")
1:
	clr     r1
1:
	jsr     pc,rmblk
		br 1f
	mov     $buf,r2
2:
	mov     (r2)+,(r1)+
	cmp     r2,$buf+BSIZE
	blo     2b
	br      1b

/ переместить память на длину ассемблерного заголовка
1:
	clr     r0
	cmp     (r0),$407
	bne     2f
1:
	mov     20(r0),(r0)+
	cmp     r0,sp
	blo     1b

/ передать управление программе и
/ перезапуститься, если она вернула управление
2:
.if     autoboot
	mov     ENDCORE-BOOTOPTS, r4
	mov     ENDCORE-BOOTDEV, r3
	mov     ENDCORE-CHECKWORD, r2
/
/ Занести в r1 реальные параметры у-ва загрузки
/ (major здесь правильный только при hardboot-е)
/
.if xautoboot
	movb    r3, r1
	bis     $[major\<8],r1
.endif
.if 1-xautoboot
	mov     $[major\<8],r1
.endif
.endif
	jsr     pc,*$0
	br      restart

/ взять I-узел заданный в r0
iget:
	bic     $140000,r0      / Clear 2 most significant bits in I-number
	add     $INOFF,r0
	mov     r0,r5
	ash     $-4.,r0
	bic     $!7777,r0
	mov     r0,dno
	clr     r0
	jsr     pc,rblk
	bic     $!17,r5
	mul     $INOSIZ,r5
	add     $buf,r5
	mov     $inod,r4
1:
	mov     (r5)+,(r4)+
	cmp     r4,$inod+INOSIZ
	blo     1b
	rts     pc

/ Читать блок файла с логическим смещением в bno.
/ Алгоритм может работать только с однокосвенными блоками.
/ Таким образом, могут быть загружены только файлы короче
/ NDIRIN+128 блоков.
rmblk:
	add     $2,(sp)
	mov     bno,r0
	cmp     r0,$NDIRIN
	blt     1f
	mov     $NDIRIN,r0
1:
	mov     r0,-(sp)
	asl     r0
	add     (sp)+,r0
	add     $addr+1,r0
	movb    (r0)+,dno
	movb    (r0)+,dno+1
	movb    -3(r0),r0
	bne     1f
	tst     dno
	beq     2f
1:
	jsr     pc,rblk
	mov     bno,r0
	inc     bno
	sub     $NDIRIN,r0
	blt     1f
	ash     $2,r0
	mov     buf+2(r0),dno
	mov     buf(r0),r0
	bne     rblk
	tst     dno
	bne     rblk
2:
	sub     $2,(sp)
1:
	rts     pc

/ Определения для RL


/ Определения для HP


/ Определения для 

hkcs1 = 177440  / РКС 1
hkda  = 177446  / номер трака/сектора
hkcs2 = 177450  / РКС 2
hkca  = 177460  / номер цилиндра

.if     rk07    / Константы для 1.
ack = 02003     /  запрос пакета
clear = 040     /  очистка подсистемы
iocom = 2021    /  читать + старт
.endif
.if     rk07-1  / Константы для RK06.
ack = 03        /  запрос пакета
clear = 040     /  очистка подсистемы
iocom = 021     /  читать + старт
.endif


/ Определения для RM


/ Определения для RK05



/ Определения для RP03


/ Определения для DVHP


/ Определения для DW (Винчестер Э85)


/ Микро-драйвер диска.
/ Мл. слово адреса в dno,
/ старшее в r0.
rblk:
	mov     r1,-(sp)


	mov     dno,r1
.if     CLSIZE-1
	ashc    $CLSHFT,r0      / умножить на CLSIZE
.endif


/------------------ RL specific



/------------------ RM specific



/------------------ HP/DVHP specific


/------------------ RK specific


/------------------  specific

	div     $22.,r0         / r0 = сектор r1 = блок
	mov     r1,-(sp)
	mov     r0,r1
	clr     r0
	div     $3.,r0          / r0 = цилиндр r1 = трак
	bisb    r1,1(sp)
.if xautoboot
	movb    ENDCORE-BOOTDEV,r1
	ash     $-3.,r1
	mov     $100000,*$hkcs1 / clear controller
	mov     r1,*$hkcs2      / unit ---> hkcs2
.if rk07
	mov     $2005,*$hkcs1   / drive clear
.endif
.if 1-rk07
	mov     $05,*$hkcs1     / drive clear
.endif
0:      tstb    *$hkcs1         / wait for CRDY
	bpl     0b
.endif
	mov     r0,*$hkca       / указать цилиндр
	mov     $hkda,r1
	mov     (sp)+,(r1)      / задать трак и сектор -> hkda
	mov     $buf,-(r1)      / адрес ОЗУ
	mov     $WC,-(r1)       / счетчик слов
	mov     $iocom,-(r1)
1:
	tstb    (r1)
	bge     1b              / ждать завершения операции
	mov     (sp)+,r1
	rts     pc


/------------------ RP specific


/------------------ DW specific


/------------------ FD specific


/------------------ RX specific





/---------------- END OF DISK-DEPENDENT PART ---------------

tks = 177560
tkb = 177562

/ читать и печатать символ с терминала
/ если *cp не содержит 0, то это - след. символ для имитации ввода
/ имени по умолчанию
getc:
	movb    *cp, r0
	beq     2f
	inc     cp

.if     readname
	br      putc
2:
	mov     $tks,r0
	inc     (r0)
1:
	tstb    (r0)
	bge     1b
	mov     tkb,r0
	bic     $!177,r0
	cmp     r0,$'A
	blo     2f
	cmp     r0,$'Z
	bhi     2f
	add     $'a-'A,r0
.endif

tps = 177564
tpb = 177566

2:
/ печатать символ на терминал
putc:
	tstb    *$tps
	bge     putc
	mov     r0,*$tpb
	cmp     r0,$'\r
	bne     1f
	mov     $'\n,r0
	br      putc
1:      rts     pc


cp:     defnm
defnm:  <boot\r\0>
end:

inod = ..-512.-BSIZE            / место для I-узла, буфера, стека
addr = inod+ADDROFF             / первый адрес в I-узле
buf  = inod+INOSIZ
bno  = buf+BSIZE
dno  = bno+2
names = dno+2
