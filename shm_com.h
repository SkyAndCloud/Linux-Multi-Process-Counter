//
// Created by yongshan on 16-9-18.
//

#ifndef LINUXIPC_SHM_COM_H
#define LINUXIPC_SHM_COM_H

#define TEXT_SZ 2048

struct shared_use_st {
    int written_by_you;
    char some_text[TEXT_SZ];
};

#endif //LINUXIPC_SHM_COM_H
