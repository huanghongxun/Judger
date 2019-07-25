import requests

UPLOAD_URL = "http://127.0.0.1:8999"

def upload(file_path, path):
    f = open(file_path, "rb")
    url = UPLOAD_URL + "/" + path
    r = requests.post(url, files={'file':f})
    print("upload {0} to {1} response {2}".format(file_path, url, r.content))


def main():
    upload("./UserTest.o", "problem/1234/support/UserTest.o")
    upload("./User.cpp", "problem/1234/support/User.cpp")
    upload("./User.hpp", "problem/1234/support/User.hpp")
    upload("./User.cpp", "submission/12340/User.cpp")

if __name__ == "__main__":
    main()
