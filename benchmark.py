#! this must be ran from the DORAM directory 
import subprocess

CIRCUIT_FILE_NAME = "LowMC_reuse_wires.txt"
NUM_QUERIES_TO_RUN = 10**5

logN_to_numlevels_ampFactor = {
    #12: [[2,3], [2,4],[2,2]],
    #13: [[2,3],[2,4], [2,5]],
    #14: [[2,4],[2,5],[3,3],[2,6]],
    15: [[3,3],[2,6]],
    #16:[[3,4]],
    #17: [[3,4],[4,3]],
    #18: [[4,3],[6,2]],
    #19: [[6,2],[3,5]],
    20: [[5,3],[4,4]],
    #21: [[5,3],[4,4]],
    #22: [[5,3], [4,5]],
    #23: [[6,3], [4,5]],
    #24: [[5,4],[6,3],[4,5]],
    25: [[5,4]],
    #26: [[7,3],[4,6]],
    #27: [[7,3],[4,6]],
    #28: [[6,4],[5,5]],
    #29: [[6,4],[5,5], [4,7]],
    30: [[4,7]],
    #31: [[7,4],[5,6],[4,7]]
}



# Call a Bash command to list all files in the current directory

for (log_N, list_of_experiments) in logN_to_numlevels_ampFactor.items():

    for num_levels, log_amp_factor in list_of_experiments:

        command = f"../benchmark/run.sh doram_array circuits --num-query-tests {NUM_QUERIES_TO_RUN} --log-address-space {log_N} --num-levels {num_levels} --log-amp-factor {log_amp_factor} --prf-circuit-filename {CIRCUIT_FILE_NAME} --build-bottom-level-at-startup {'false' if (1<<log_N)>100000 else 'true'}"
        print(command, '\n')
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)

        # Read the output and error streams
        output, error = process.communicate()

        # Print the output and error messages
        print(output.decode('utf-8'))
        print(error.decode('utf-8'))



