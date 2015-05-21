################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Global/FileManager/FileDataReceiver.c \
../Global/FileManager/FileDataSender.c \
../Global/FileManager/FileTransferController.c 

OBJS += \
./Global/FileManager/FileDataReceiver.o \
./Global/FileManager/FileDataSender.o \
./Global/FileManager/FileTransferController.o 

C_DEPS += \
./Global/FileManager/FileDataReceiver.d \
./Global/FileManager/FileDataSender.d \
./Global/FileManager/FileTransferController.d 


# Each subdirectory must supply rules for building sources it contributes
Global/FileManager/%.o: ../Global/FileManager/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


