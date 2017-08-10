//
// Created by yongshan on 16-9-18.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MAX_RANGE       ((long)2<<31)  //累加范围的最大限制
#define MAX_COUNTER_NUM 100            //最大进程数
#define INITIAL_CUR     1              //初始化cur
#define INITIAL_SUM     0              //初始化sum
#define INPUT_FILE      "input.txt"    //输入
#define OUTPUT_FILE     "output.txt"   //输出
#define TIME_FILE       "timetest.csv"   //保存时间测试结果的文件

//用于信号量操作的联合
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct adder {
    long long cur;  //下一个被累加的数值
    long long sum;  //当前总和
};

int set_semvalue(void);
void del_semvalue(void);
int semaphore_p(void);
int semaphore_v(void);

int sem_id;

int main(void) {
    FILE* fp;
    struct adder *shm;
    int counter_num;
    long range;
    pid_t pid;
    void *shared_memory = (void *)0;
    int shmid;
    struct timeval start;   //记录开始测试时间
    struct timeval end;     //记录结束测试的时间

    //读取配置文件并做错误处理
    if ((fp = fopen(INPUT_FILE, "r")) == NULL) {
        fprintf(stderr, "ERROR: open %s failed.\n", INPUT_FILE);
        exit(EXIT_FAILURE);
    }
    rewind(fp);
    if (fscanf(fp, " N = %d", &counter_num) != 1) {
        fprintf(stderr, "ERROR: read N failed.\n");
        exit(EXIT_FAILURE);
    } else if (fscanf(fp, " M = %ld", &range) != 1) {
        fprintf(stderr, "ERROR: read M failed.\n");
        exit(EXIT_FAILURE);
    } else if (range > MAX_RANGE || counter_num > MAX_COUNTER_NUM) {
        fprintf(stderr, "ERROR: invalid args N=%d M=%ld\n", counter_num, range);
        exit(EXIT_FAILURE);
    } else {
        printf("args N=%d M=%ld\n", counter_num, range);
    }
    fclose(fp);

    sem_id = semget((key_t)1234, 1, 0666 | IPC_CREAT);
    if (!set_semvalue()) {
        fprintf(stderr, "ERROR: failed to initialize semaphore\n");
        exit(EXIT_FAILURE);
    }

    //设置共享内存
    shmid = shmget((key_t)1235, sizeof(struct adder), 0666 | IPC_CREAT);
    if (shmid == -1) {
        fprintf(stderr, "ERROR: shmget failed\n");
        exit(EXIT_FAILURE);
    }
    shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1) {
        fprintf(stderr, "ERROR: shmat failed\n");
        exit(EXIT_FAILURE);
    }
    shm = (struct adder *)shared_memory;
    shm->cur = INITIAL_CUR;
    shm->sum = INITIAL_SUM;
    printf("from %d, cur is %lld, sum is %lld\n", getpid(), shm->cur, shm->sum);

    //记录开始测试的时间
    gettimeofday(&start, NULL);
    for (int i = 0; i < counter_num; i++) {
        pid = fork();
        if (pid == -1) {
            fprintf(stderr, "ERROR: fork has failed.\n");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            //子进程
            shared_memory = shmat(shmid, (void *)0, 0);
            if (shared_memory == (void *)-1) {
                fprintf(stderr, "ERROR: shmat failed\n");
                exit(EXIT_FAILURE);
            }
            shm = (struct adder *)shared_memory;
            while (1) {
                if (!semaphore_p()) {
                    fprintf(stderr, "ERROR: semaphore_p has failed. pid is %d\n", getpid());
                    exit(EXIT_FAILURE);
                }
                if (shm->cur > range) {
                    //计算已完成，退出
                    if (!semaphore_v()) {
                        fprintf(stderr, "ERROR: semaphore_v has failed. pid is %d\n", getpid());
                        exit(EXIT_FAILURE);
                    }
                    if (shmdt(shared_memory) == -1) {
                        fprintf(stderr, "ERROR: shmdt failed. pid is %d\n", getpid());
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                } else {
                    //累加
                    shm->sum += shm->cur;
                    (shm->cur)++;
                    printf("from %d, now sum is %lld\n", getpid(), shm->sum);
                }
                if (!semaphore_v()) {
                    fprintf(stderr, "ERROR: semaphore_v has failed. pid is %d\n", getpid());
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            //父进程
        }
    }

    //等待子进程结束
    while (wait(NULL) > 0)
        ;

    //记录结束测试的时间
    gettimeofday(&end, NULL);

    //输出结果到文件
    if ((fp = fopen(OUTPUT_FILE, "w")) == NULL) {
        fprintf(stderr, "ERROR: open %s failed\n", OUTPUT_FILE);
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%lld", shm->sum);
    fflush(fp);
    fclose(fp);

    //输出时间测试结果
    if ((fp = fopen(TIME_FILE, "a")) == NULL) {
        fprintf(stderr, "ERROR: open %s failed\n", TIME_FILE);
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%d,%ld,%ld\n", counter_num, range, ((end.tv_usec - start.tv_usec) + (end.tv_sec - start.tv_sec) * (1<<6)));
    fflush(fp);
    fclose(fp);

    del_semvalue();
    if (shmdt(shared_memory) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "shmctl(IPC_RMID) failed\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
//设置信号量
int set_semvalue(void) {
    union semun sem_union;
    sem_union.val = 1;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1)
        return 0;
    return 1;
}
//删除信号量
void del_semvalue(void) {
    union semun sem_union;
    if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1)
        fprintf(stderr, "Failed to delete semaphore\n");
}
//信号量p操作
int semaphore_p(void) {
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = -1;
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_p failed\n");
        return 0;
    }
    return 1;
}
//信号量v操作
int semaphore_v(void) {
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = 1;
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_v failed\n");
        return 0;
    }
    return 1;
}