################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Global/Transport/NetworkConnection.c 

OBJS += \
./Global/Transport/NetworkConnection.o 

C_DEPS += \
./Global/Transport/NetworkConnection.d 


# Each subdirectory must supply rules for building sources it contributes
Global/Transport/%.o: ../Global/Transport/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


