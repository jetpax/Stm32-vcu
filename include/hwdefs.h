/*
 * This file is part of the ZombieVerter project.
 *
 * Copyright (C) 2019-2022 Damien Maguire <info@evbmw.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HWDEFS_H_INCLUDED
#define HWDEFS_H_INCLUDED


//Common for any config

//Maximum value for over current limit timer
#define GAUGEMAX 4096
#define USART_BAUDRATE 115200
//Maximum PWM frequency is 36MHz/2^MIN_PWM_DIGITS
#define MIN_PWM_DIGITS 11
#define PERIPH_CLK      ((uint32_t)36000000)

#if defined(STM32F1)

#define RCC_CLOCK_SETUP rcc_clock_setup_in_hse_8mhz_out_72mhz

#elif defined(STM32F4)

#define RCC_CLOCK_SETUP rcc_clock_setup_pll(&rcc_hse_12mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

#endif

#define PWM_TIMER     TIM1
#define PWM_TIMRST    RST_TIM1
#define PWM_TIMER_IRQ NVIC_TIM1_UP_IRQ
#define pwm_timer_isr tim1_up_isr

#define FUELGAUGE_TIMER TIM4

#ifdef H_Z      // Headless uses USART 1 for term, SPI1 for CAN

#define TERM_USART         USART1


#define SPI_CAN             SPI1

#define CAN3_CS_PORT        GPIOC
#define CAN3_CS_PIN         GPIO2

#define CAN3_INT_PORT       GPIOC
#define CAN3_INT_PIN        GPIO8

#define CAN3_EXTI           EXTI8
#define CAN3_EXTI_VECTOR    NVIC_EXTI9_5_IRQ
#define CAN3_ISR   exti9_5_isr


#else           // Zombieverter uses USART3, SPI2

#define TERM_USART         USART3

#define TERM_BUFSIZE       128

#define SPI_CAN             SPI2

#define CAN3_CS_PORT        GPIOB
#define CAN3_CS_PIN         GPIO12

#define CAN3_INT_PORT       GPIOE
#define CAN3_INT_PIN        GPIO15

#define CAN3_EXTI           EXTI15
#define CAN3_EXTI_VECTOR    NVIC_EXTI15_10_IRQ
#define  CAN3_ISR  exti15_10_isr

#endif

#define DRV8912_CS_PORT     GPIOC
#define DRV8912_CS_PIN      GPIO1

#define TIC12400_CS_PORT    GPIOC
#define TIC12400_CS_PIN     GPIO0

#define TIC12400_INT_PORT   GPIOC
#define TIC12400_INT_PIN    GPIO13


//Address of parameter block in flash for 105
#define FLASH_PAGE_SIZE 2048
#define PARAM_BLKNUM  1
#define PARAM_BLKSIZE FLASH_PAGE_SIZE
#define CAN1_BLKNUM   2
#define CAN2_BLKNUM   4

#ifdef STM32F4
#define FLASH_SECTOR_0 0
#define FLASH_SECTOR_1 1
#define FLASH_SECTOR_2 2
#define FLASH_SECTOR_3 3
#define FLASH_SECTOR_4 4
#define FLASH_SECTOR_5 5
#define FLASH_SECTOR_6 6
#define FLASH_SECTOR_7 7
#define FLASH_SECTOR_8 8
#define FLASH_SECTOR_9 9
#define FLASH_SECTOR_10 10
#define FLASH_SECTOR_11 11
#endif

#endif // HWDEFS_H_INCLUDED
