import requests

UPLOAD_URL = "http://127.0.0.1:8999"

def upload(file_path, path):
    f = open(file_path, "rb")
    url = UPLOAD_URL + "/" + path
    r = requests.post(url, files={'file':f})
    print("upload {0} to {1} response {2}".format(file_path, url, r.content))


def main():
    upload("./support/main.cpp", "problem/1234/support/main.cpp")
    upload("./stand_input0.in", "problem/1234/standard_input/input0")
    upload("./stand_output0.out", "problem/1234/standard_output/output0")
    upload("./stand_input1.in", "problem/1234/standard_input/input1")
    upload("./stand_output1.out", "problem/1234/standard_output/output1")
    upload("./stand_input2.in", "problem/1234/standard_input/input2")
    upload("./stand_output2.out", "problem/1234/standard_output/output2")
    upload("./main.cpp", "submission/12340/main.cpp")

if __name__ == "__main__":
    main()
