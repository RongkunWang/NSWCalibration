import argparse
import os
import subprocess
import sys

is_failed = False

def get_tests():
    """ Gets all files matching the following criteria in the current working directory.
        Matching files start with the character sequence "test_", have no filename extension, and are executable.
    """
    for node in os.listdir("."):
        if (os.path.isfile(node)):
            if(os.access(node, os.X_OK) and node.startswith("test") and node.find(".") == -1):
                yield node

def run_test(executable):
    """ Runs the given executable, filtering out XML header.
    """
    report_filename = f"{executable}_report.xml"
    proc = subprocess.Popen(f"./{executable} --catch_system_error=yes --report_level=no --log_format=JUNIT --log_sink={report_filename}", shell=True, stdout=subprocess.PIPE)
    # 30 second timeout until termination
    proc.wait(30)
    if (proc.returncode != 0):
        global is_failed
        is_failed = True

    out = ""
    with open(report_filename, "r") as report_file:
        report_file.readline()      # skip XML header
        line = report_file.readline()
        while line:
            out += line
            line = report_file.readline()
    os.remove(report_filename)      # delete file created by test
    return out

def write_report(reports, output_file):
    """ Collates the report strings in the reports list to output_file, prepending XML metadata.
    """
    with open(output_file, "w+") as outfile:
        outfile.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
        outfile.write("<testsuites>\n")
        for report in reports:
            outfile.write(report + "\n")
        outfile.write("</testsuites>\n")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--test_directory", help="The directory where the test files are found", required=True)
    parser.add_argument("-o", "--output_file", help="Filepath for the report XML to be written to", required=True)
    args = parser.parse_args()

    prev_wd = os.getcwd()

    os.chdir(args.test_directory)
    print(f"Running unit tests in {os.getcwd()}")
    reports = []
    for filename in get_tests():
        print(f"\tRunning test executable {filename}")
        reports.append(run_test(filename))

    os.chdir(prev_wd)
    print(f"Writing report to {args.output_file}")
    write_report(reports, args.output_file)
    if (is_failed):
        print("One or more tests failed, setting exit code to 1!")
        sys.exit(1)

if __name__ == "__main__":
    main()
