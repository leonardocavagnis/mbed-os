/*
 * Copyright 2022 Arduino SA
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "i2c_a7.h"
#include "mbed.h"

static I2C * se05x_i2c;

i2c_error_t axI2CInit(void **conn_ctx, const char *pDevName)
{
    se05x_i2c = new I2C(PB_7, PB_6);
    if(se05x_i2c != NULL) 
    {
        se05x_i2c->frequency(400000);
        return I2C_OK;
    }
    return I2C_FAILED;
}

void axI2CTerm(void* conn_ctx, int mode)
{
    if(se05x_i2c != NULL) 
    {
        delete se05x_i2c;
    }
}

i2c_error_t axI2CWrite(void* conn_ctx, unsigned char bus, unsigned char addr, unsigned char * pTx, unsigned short txLen)
{
    if(se05x_i2c->write(addr, (const char *)pTx, txLen))
    {
        return I2C_FAILED;
    }
    return I2C_OK;
}

i2c_error_t axI2CRead(void* conn_ctx, unsigned char bus, unsigned char addr, unsigned char * pRx, unsigned short rxLen)
{
    if(se05x_i2c->read(addr, (char *)pRx, rxLen))
    {
        return I2C_FAILED;
    }
    return I2C_OK;
}

