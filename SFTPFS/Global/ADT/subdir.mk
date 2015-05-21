################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Global/ADT/DArray.c 

OBJS += \
./Global/ADT/DArray.o 

C_DEPS += \
./Global/ADT/DArray.d 


# Each subdirectory must supply rules for building sources it contributes
Global/ADT/%.o: ../Global/ADT/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


