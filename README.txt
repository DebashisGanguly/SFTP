==================================================================================
                                      README                                      
==================================================================================
CS2520 - Project
Simple File Transfer Protocol
Group Members:
	Debashis Ganguly (DEG80)
	Darshan Shetty (DBS19)
==================================================================================

Files Included:
- "SFTP"ù a directory that contains the source code files, make files, StatAnalysislog, Config file, name_server_info.txt
- "Config" a directory that contains the configuration file: sftp.conf to specify below configurable items:
	name server info file path, 
	window size, 
	time out, 
	number of attempts,
	MTU,
	Inject error flag, 
	Probability of error, 
	Percentage of congestion, 
	Logical name for file server, 
	path for statistical analysis log file.
- Place the "Config" folder at the desired directory, and update the full path "SFTP.h" header file in the source code.
- "StatAnalysislog" a directory which contains the Stat_Analysis.log file to collect the experiment readings.
- README file
- "Experiments.xlsx" All the experiments listed with the steps done and referring the graph in Project Report.

Running the executable:
Steps to run and test, make file is created for each module in the corresponding folders:
1. Go to SFTPNS folder, make file is created.
2. SFTPNS (run the name server, can run multiple name servers)
3. Go to SFTPFS folder.
4. SFTPFS 	(run the file server, can run multiple file servers)
5. Go to SFTPClient folder
6. SFTPClient	(run the client)
7. Press ? for help on commands. Use the desired command.
8. Issue quit to exit the application.
