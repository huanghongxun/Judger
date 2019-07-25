import requests

UPLOAD_URL = "http://127.0.0.1:8999"

def upload(file_path, path):
    f = open(file_path, "rb")
    url = UPLOAD_URL + "/" + path
    r = requests.post(url, files={'file':f})
    print("upload {0} to {1} response {2}".format(file_path, url, r.content))


def main():
    upload("./test.cpp", "problem/1234/support/test.cpp")
    upload("./adder.cpp", "problem/1234/support/adder.cpp")
    upload("./adder.hpp", "problem/1234/support/adder.hpp")
    upload("./adder.cpp", "submission/12340/adder.cpp")

if __name__ == "__main__":
    main()
