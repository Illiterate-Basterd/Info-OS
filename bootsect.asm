.code16
.org 0x7C00

start:
	
	mov %cs, %ax 
	mov %ax, %ds
	mov %ax, %ss
	mov $start, %sp

	mov $0x1000, %bx
	mov %bx, %es
	mov $0x0, %bx
	mov $0, %ch #Nomer Tsilindra
	mov $1, %cl #Nomer sektora
	mov $1, %dl #Nomer diska
	mov $0, %dh #Nomer golovki
	
	mov $0x02, %ah
	mov $0x20, %al
	int $0x13

	mov $0x02, %ah
	int $0x1A
	add $3, %ch
	mov %dh, 0x568 #Seconds
	mov %cl, 0x548 #Minutes
	mov %ch, 0x528 #Hours

	# Отключение прерываний
	cli
	
	# Загрузка размера и адреса таблицы дескрипторов
	lgdt gdt_info

	# Включение адресной линии А20
	inb $0x92, %al
	orb $2, %al
	outb %al, $0x92

	# Установка бита PE регистра CR0 - процессор перейдет в защищенный режим
	movl %cr0, %eax
	orb $1, %al
	movl %eax, %cr0

	ljmp $0x8, $protected_mode

#Global Descriptor Table

gdt:
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	.byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
	.byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00

gdt_info:
	.word gdt_info - gdt
	.word gdt, 0

	

.code32
protected_mode:
	movw $0x10, %ax
	movw %ax, %es
	movw %ax, %ds
	movw %ax, %ss
	call 0x10000
	
	.zero (512 - (. - start) - 2)
	.byte 0x55, 0xAA
