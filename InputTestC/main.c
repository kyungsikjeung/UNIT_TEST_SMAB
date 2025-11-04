#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


#include "sch.h"
/* ===== 초기화 및 등록 ===== */

int main(){
    init_task();      // 태스크 슬롯 초기화
    bool isExit = false;
    int cnt = 0;  // ★ 루프 밖으로 이동
    while(!isExit){
        test_isr();
        run_tasks();
        cnt++;
        if(cnt > 20000){ // 20초 후 종료
            isExit = true;
        }
    }

    return 0;
}