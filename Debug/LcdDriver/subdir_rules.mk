################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
LcdDriver/Crystalfontz128x128_ST7735.obj: ../LcdDriver/Crystalfontz128x128_ST7735.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: MSP430 Compiler'
	"/Applications/ti/ccsv7/tools/compiler/ti-cgt-msp430_16.9.6.LTS/bin/cl430" -vmspx --data_model=restricted --use_hw_mpy=F5 --include_path="/Users/bobby/workspace_v7/ECE554_C/driverlib/MSP430F5xx_6xx" --include_path="/Users/bobby/workspace_v7/ECE554_C/fonts" --include_path="/Users/bobby/workspace_v7/ECE554_C/LcdDriver" --include_path="/Users/bobby/workspace_v7/ECE554_C/grlib" --include_path="/Applications/ti/ccsv7/ccs_base/msp430/include" --include_path="/Users/bobby/workspace_v7/ECE554_C" --include_path="/Applications/ti/ccsv7/tools/compiler/ti-cgt-msp430_16.9.6.LTS/include" --advice:power=all --define=__MSP430F5529__ -g --printf_support=minimal --diag_warning=225 --diag_wrap=off --display_error_number --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --preproc_with_compile --preproc_dependency="LcdDriver/Crystalfontz128x128_ST7735.d_raw" --obj_directory="LcdDriver" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

LcdDriver/HAL_MSP_EXP432P401R_Crystalfontz128x128_ST7735.obj: ../LcdDriver/HAL_MSP_EXP432P401R_Crystalfontz128x128_ST7735.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: MSP430 Compiler'
	"/Applications/ti/ccsv7/tools/compiler/ti-cgt-msp430_16.9.6.LTS/bin/cl430" -vmspx --data_model=restricted --use_hw_mpy=F5 --include_path="/Users/bobby/workspace_v7/ECE554_C/driverlib/MSP430F5xx_6xx" --include_path="/Users/bobby/workspace_v7/ECE554_C/fonts" --include_path="/Users/bobby/workspace_v7/ECE554_C/LcdDriver" --include_path="/Users/bobby/workspace_v7/ECE554_C/grlib" --include_path="/Applications/ti/ccsv7/ccs_base/msp430/include" --include_path="/Users/bobby/workspace_v7/ECE554_C" --include_path="/Applications/ti/ccsv7/tools/compiler/ti-cgt-msp430_16.9.6.LTS/include" --advice:power=all --define=__MSP430F5529__ -g --printf_support=minimal --diag_warning=225 --diag_wrap=off --display_error_number --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --preproc_with_compile --preproc_dependency="LcdDriver/HAL_MSP_EXP432P401R_Crystalfontz128x128_ST7735.d_raw" --obj_directory="LcdDriver" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


