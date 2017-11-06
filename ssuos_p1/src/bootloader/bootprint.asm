[BITS 16]
ORG 0x7c00
START:   
mov     ax, 0xb800 ; 텍스트 출력용 메모리
mov     es, ax
mov     ax, 0x00
mov     bx, 0
mov     cx, 80*25*2 ; 반복문

CLS:
mov     [es:bx], ax ;es
add     bx, 1
loop CLS        ;; 화면 초기화

mov     ax, 0xb800
mov     es, ax ; es초기화
mov     bx, 0 ; bx 초기화
mov     ah, 0x07  ;픽셀 글씨 색 또는 배경 굵기 깜빡임 등
lea     si, [HELLO_MSG] ; si에 Hello_msg의 주소값 집어넣기

PRINT:
mov     al, [si] ;; al은 하위 1바이트 ah는 상위 1바이트
inc     si ; si를 1올리기
mov     [es:bx], ax ; es + bx의 위치에 ax넣기
add     bx, 2
cmp     al, 0 ; al이 비어있으면 zf가 1이되고 값이 존재하면 zf는 0이다.
jnz     PRINT ; zf가 0일때 즉 값이 존재할때 PRINT로 jump


HELLO_MSG:
db "Hello, Yujung's World", 0x00
