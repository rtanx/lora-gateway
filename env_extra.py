from os.path import isfile
Import('env')  # type: ignore

assert isfile('.env')
try:
    f = open('.env')
    lines = f.readlines()
    envs = []
    for line in lines:
        print("'-D {}'".format(line.strip()))
        envs.append("'-D {}'".format(line.strip()))
    env.Append(BUILD_FLAGS=envs)  # type: ignore
except IOError:
    print("File .env not accessible or not exists",)
finally:
    f.close()
