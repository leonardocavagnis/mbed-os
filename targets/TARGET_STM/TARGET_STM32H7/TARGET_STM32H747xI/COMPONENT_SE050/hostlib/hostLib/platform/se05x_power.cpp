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

#include "se05x_apis.h"
#include "sm_timer.h"
#include "mbed.h"

static DigitalOut se05x_ic_enable(MBED_CONF_TARGET_SE050_ENA, 0);

void se05x_ic_reset(void)
{
    se05x_ic_power_off();
    sm_sleep(100);
    se05x_ic_power_on();
}

void se05x_ic_power_on(void)
{
    se05x_ic_enable = 1;
}

void se05x_ic_power_off(void)
{
    se05x_ic_enable = 0;
}
