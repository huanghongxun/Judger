import requests

UPLOAD_URL = "http://127.0.0.1:8999"

def upload(file_path, path):
    f = open(file_path, "rb")
    url = UPLOAD_URL + "/" + path
    r = requests.post(url, files={'file':f})
    print("upload {0} to {1} response {2}".format(file_path, url, r.content))


def main():
    upload("./fake_file/main.cpp", "problem/1234/support/main.cpp")
    upload("./fake_file/header.hpp", "problem/1234/support/header.hpp")
    upload("./fake_file/random_gen.py", "problem/1234/random_source/random_gen.py")
    upload("./fake_file/stand_header.h", "problem/1234/support/stand_header.h")
    upload("./fake_file/stand_main.cpp", "problem/1234/support/stand_main.cpp")
    upload("./fake_file/stand_input.in", "problem/1234/standard_input/input0")
    upload("./fake_file/stand_output.out", "problem/1234/standard_output/output0")
    upload("./fake_file/stand_input1.in", "problem/1234/standard_input/input1")
    upload("./fake_file/stand_output1.out", "problem/1234/standard_output/output1")
    upload("./fake_file/sub_header.h", "submission/12340/sub_header.h")
    upload("./fake_file/sub_main.cpp", "submission/12340/sub_main.cpp")

if __name__ == "__main__":
    main()
