# template-c Repository Guide

Welcome to the `c template` repository. This guide will help you set up and run the provided scripts.

## **Table of Contents**

1. [Cloning the Repository](#cloning-the-repository)
2. [Prerequisites](#Prerequisites)
3. [Running the `generate-cmakelists.sh` Script](#running-the-generate-cmakelistssh-script)
4. [Running the `change-compiler.sh` Script](#running-the-change-compilersh-script)
5. [Running the `build.sh` Script](#running-the-buildsh-script)
6. [Running the `build-all.sh` Script](#running-the-build-allsh-script)
7. [Copy the template to start a new project](#copy-the-template-to-start-a-new-project)
8. [Testing the Code](#testing-the-code)

## **Cloning the Repository**

Clone the repository using the following command:

```bash
git clone https://github.com/programming101dev/template-c.git
```

Navigate to the cloned directory:

```bash
cd template-c
```

Ensure the scripts are executable:

```bash
chmod +x *.sh
```

## **Prerequisites**

- to ensure you have all of the required tools installed, run:
```bash
./check-env.sh
```

If you are missing tools follow these [instructions](https://docs.google.com/document/d/1ZPqlPD1mie5iwJ2XAcNGz7WeA86dTLerFXs9sAuwCco/edit?usp=drive_link).

## **Running the generate-cmakelists.sh Script**

You will need to create the CMakeLists.txt file:

```bash
./generate-cmakelists.sh
```

## **Running the change-compiler.sh Script**

Tell CMake which compiler you want to use:

```bash
./change-compiler.sh -c <compiler>
```

To the see the list of possible compilers:

```bash
cat supported_cxx_compilers.txt
```

## **Running the build.sh Script**

To build the program run:

```bash
./build.sh
```

## **Running the build-all.sh Script**

To build the program with all compilers run:

```bash
./build-all.sh
```

## **Copy the template to start a new project**

To create a new project from the template, run:

```bash
./copy-template.sh <desitnation directory>
```

This will copy all of the files needed to start a new project.

1. Edit the files.txt
2. run ./generate-cmakelists.sh
3. run ./change-compiler.sh -c <compiler>
4. run ./build.sh

The files.txt file contains:
<executable> <source files> <header files> <libraries>

When you need to add/removes files to/from the project you must rerun the 4 steps above.

## **Testing the Code**

1. Build as usual
2. Generate share library: gcc -shared -fPIC src/handler_v1.c src/utils.c src/db.c src/stringTools.c -Iinclude -o ../data/handler/handler_v1.so
3. On first terminal, run ./app -t server -i <ip> -p <port> (example ./app -t server -i 192.168.21.128 -p 8000)
4. On second terminal, choosing option to test (install netcat):

        HEAD: echo -e "HEAD /index.html HTTP/1.1\r\nHost: 192.168.21.128:8000\r\nConnection: keep-alive\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n\r\n" | nc 192.168.21.128 8000
   
    
        GET: echo -e "GET /index.html HTTP/1.1\r\nHost: 192.168.21.128:8000\r\nConnection: keep-alive\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n\r\n" | nc 192.168.21.128 8000

    
        POST: echo -e "POST /submit HTTP/1.1\r\nHost: 192.168.21.128:8000\r\nConnection: keep-alive\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 23\r\n\r\nkey1=kiet&key2=ngo" | nc 192.168.21.128 8000

        Check POST successfully with:     echo -e "GET db/post_data.db.pag HTTP/1.1\r\nHost: 192.168.21.128:8000\r\nConnection: keep-alive\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n\r\n" | nc 192.168.21.128 8000
