# isa23ldap-server

## 📚 Introduction

Primitive LDAPv2 server ISA project from 2023.

### Usage 

`sudo ./isa-ldapserver -p 389 -f database.csv`

## 📃 Theory
LDAP, or Lightweight Directory Access Protocol, is a protocol designed to access and manage distributed directory information. It is a standardized open protocol application protocol that allows clients to query and update data in a hierarchical directory. LDAP is widely used to manage information about users, computers, network devices, and other objects within an organization.

The LDAP protocol is based on the X.500 recommendation, which was developed in the ISO/OSI world, but has completely taken off in practice, mainly due to its "size" and subsequent "heaviness". [1]

### Hierarchical Structure
* Data in LDAP is organized into a tree structure called a directory tree.
* Each node of this tree is identified by a unique name - Distinguished Name (DN).
* DN represents the path from the root of the tree to a given node.

### Basic principles
1. Database Model
    - Data in LDAP is stored in the form of records that contain attributes and their values.
2. LDAP Server
    - Provides services for reading and writing to the directory.
    - Manages data hierarchy and responds to client inquiries.
3. LDAP Clients
    - They communicate with the LDAP server using the LDAP protocol.
    - They can perform various operations such as searching, adding, deleting and modifying data.
4. LDAP Distinguished Name (DN)
    - The unique identifier of the node in the directory tree.
    - It consists of the names of individual nodes from the root of the tree.

### Examples Using LDAP
1. Administration of User Accounts
    - Keeping information about users, including their name, password and access rights.
2. Network Resource Management
    - Logging of network devices such as servers, printers and routers.
3. Centralized Directory Management
    - Storage of contact information about employees, departments and other organizational units.
4. Implementation of SSO (Single Sign-On)
    - Using LDAP to authenticate users and provide single sign-on.
  
The project was working with LDAP version 2 and the operation was read only. The server runs on port 389 and the command: "ldapsearch" is entered to communicate with the server, see examples of statements.
The server thus responds only with the parameters uid, cn and mail. [4]

### Basic Encoding Rules
Basic Encoding Rules (BER) specify how data should be encoded for transmission, independent of machine type, programming language, or application program representation.

BER uses the Tag-Length-Value (TLV) format to encode information. The type or tag indicates what kind of data follows, the length indicates the length of the data that follows, and the value represents the actual data. Each value can consist of one or more TLV-encoded values, each with its own identifier, length, and content.

Although TLV encoding increases the number of octets transferred, it makes it easier to introduce new fields into messages that even older implementations can handle. Another advantage is that the predictability of the coding makes debugging easier. [3]

Each incoming message must follow a predetermined format. E.g. `bindRequest` It always has a sequence label, followed by the length of the sequence, `messegeID`, the label of the bindRequest, the length of the message and the rest of the message.
Accordingly, bindResponse can also be fixed.

The first octet indicates that it is a sequence and the second the rest of it. `0x02` `0x01` `0x01`
indicates the messageID (`0x02` number label, `0x01` length and `0x01` value). `0x61` is the `bindResponse` label and `0x07` is its length. The rest is a message that contains things like authentication.

This is how `searchResEntry` is subsequently compiled, only much more complicated.

## 📇 Code

### TCP connection

After processing the arguments, a tcp connection is set up. There is a tcp function for this which was taken from NESFIT, IPK projects. [5] In the function, a socket is created and bound to, and then it listens. Once it catches a `bindRequest` packet, it responds with a `bindResponse` and transitions to the searchRequest waiting state. After it is captured, the `get_message` function is called.

### Incoming message processing

Since all incoming messages have a fixed format, the position of individual parameters can be calculated. This is done by the `get_message` function. At the same time, it calls the function `search_database`, which searches the file with records, and send_response, which generates a `searchResEntry`. `search_database` opens the file, divides it into 3 parts using regex and, using the filter parameter, stores (using the `append_attributes` function) in the structure only those records where the filters match. `send_response`, it will first process and find out what attributes are specified. They then append to the result string and append the rest of the message as well.

### Auxiliary functions

The program contains some helper functions, like `concat_hex_array` for concatenation, or `int_to_string` for converting a number to its hex state (`4 -> 0x04`) and connecting to result.

### Structures

The first structure is the database. This is used to filter the required records from the file.
The second is attributes. It also has a pointer to the following structure. When compiling `searchResDone`, a list of attribute structures is created and sent to the client one by one in the while. Allowed attributes are `mail`, `uid` and `cn`.
It is important to take into account that the entire program does not store information from the specified file in a variable, but goes through the lines each time and stores the necessary records in structures and then releases them.

## ⚠️ Shortcomings of my program

The shortcomings of the program are: it does not support the and, or and not operations, with the substring 'dn' part in the result, it displays nonsense (caused by my lack of understanding of the part), at the same time with the substring with some specific substring (I don't know what and I couldn't imitate the situation), searchResEntry contains flats that are negative.

## 📄 Testing
For sample testing, here are 5 examples. My `database.csv` file contains just over 130 records. It was tested locally on port 389, but I could only run it on Linux from port `2001`. I tried to combine parameters.
The program supports: limit setting (when the limit is set, or when it is omitted, the limit is infinite, just like on the school LDAP server), attribute setting (if a meaningless attribute is entered, the attribute is ignored), filter setting (`substring` and `equalMatch`).
Commands are marked with `$`.

I tested it by starting the ldap server in one console:

`sudo ./isa-ldapserver -p 389 -f database.csv`

and in the second I entered the commands:

1. List 5 records without filter and attributes
   
```
$ ldapsearch -H ldap://localhost:389 -x -z 5 -b "dc=vutbr,dc=cz"

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: (objectclass=*)
# requesting: ALL
#

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
uid: xfeile00
mail: xfeile00

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
uid: xfelkl00
mail: xfelkl00

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
uid: xfiala41
mail: xfiala41

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
uid: xfiala38
mail: xfiala38

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
uid: xfiedo00
mail: xfiedo00

# search result
search: 2
result: 0 Success

# numResponses: 6
# numEntries: 5
```

2. List 5 records with uid = xvorob02
```
$ ldapsearch -H ldap://localhost:389 -x -z 5 -b "dc=vutbr,dc=cz" 'uid=xvorob02'

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: uid=xvorob02
# requesting: ALL
#

# xvorob02, fit.vutbr.cz
dn: uid=xvorob02,dc=fit,dc=vutbr,dc=cz
uid: xvorob02
mail: xvorob02

# search result
search: 2
result: 0 Success

# numResponses: 2
# numEntries: 1
```

3. Write records where uid = xv*2 (here is unexpected output in dn, correctly there should be uid and location)

```
$ ldapsearch -H ldap://localhost:389 -x -b "dc=vutbr,dc=cz" 'uid=xv*2'

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: uid=xv*2
# requesting: ALL
#

#
dn:: dWlkPYACeHaCATIsZGM9Zml0LGRjPXZ1dGJyLGRjPWN6
uid: xvorob02
mail: xvorob02

#
dn:: dWlkPYACeHaCATIsZGM9Zml0LGRjPXZ1dGJyLGRjPWN6
uid: xvrana32
mail: xvrana32

# search result
search: 2
result: 0 Success

# numResponses: 3
# numEntries: 2
```

4. Write your name and email, where uid = xvorob02

```
$ ldapsearch -H ldap://localhost:389 -x -b "dc=vutbr,dc=cz" 'uid=xvorob02' mail cn

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: uid=xvorob02
# requesting: mail cn
#

# xvorob02, fit.vutbr.cz
dn: uid=xvorob02,dc=fit,dc=vutbr,dc=cz
cn: Vorobec Valentyn
mail: xvorob02

# search result
search: 2
result: 0 Success

# numResponses: 2
# numEntries: 1
```

5. Write the name and email, where uid = xv*b*2 (here is an unexpected output in dn, correctly there should be uid and location)

```
$ ldapsearch -H ldap://localhost:389 -x -b "dc=vutbr,dc=cz" 'uid=xv*b*2' mail
cn

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: uid=xv*b*2
# requesting: mail cn
#

#
dn:: dWlkPYACeHaBAWKCATIsZGM9Zml0LGRjPXZ1dGJyLGRjPWN6
cn: Vorobec Valentyn
mail: xvorob02

# search result
search: 2
result: 0 Success

# numResponses: 2
# numEntries: 1
```

6. List 5 records where there is a mail attribute and a meaningless attribute ll

```
$ ldapsearch -H ldap://localhost:389 -x -z 5 -b "dc=vutbr,dc=cz" mail ll

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: (objectclass=*)
# requesting: mail ll
#

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
mail: xfeile00

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
mail: xfelkl00

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
mail: xfiala41

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
mail: xfiala38

# fit.vutbr.cz
dn: dc=fit,dc=vutbr,dc=cz
mail: xfiedo00

# search result
search: 2
result: 0 Success

# numResponses: 6
# numEntries: 5
```

7. List 5 records, without -b (here there is no output in dn, correctly there should be uid and location)

```
$ ldapsearch -H ldap://localhost:389 -x -z 5

# extended LDIF
#
# LDAPv3
# base <> (default) with scope subtree
# filter: (objectclass=*)
# requesting: ALL
#

#
dn: dc=fit,
uid: xfeile00
mail: xfeile00

#
dn: dc=fit,
uid: xfelkl00
mail: xfelkl00

#
dn: dc=fit,
uid: xfiala41
mail: xfiala41

#
dn: dc=fit,
uid: xfiala38
mail: xfiala38

#
dn: dc=fit,
uid: xfiedo00
mail: xfiedo00

# search result
search: 2
result: 0 Success

# numResponses: 6
# numEntries: 5
```

8. Example when the limit is 0 (in that case ignore the limit, just like with the school ldap server)

```
$ ldapsearch -H ldap://localhost:389 -x -z 0 -b "dc=vutbr,dc=cz" 'uid=xv*2'

# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: uid=xv*2
# requesting: ALL
#

#
dn:: dWlkPYACeHaCATIsZGM9Zml0LGRjPXZ1dGJyLGRjPWN6
uid: xvorob02
mail: xvorob02

#
dn:: dWlkPYACeHaCATIsZGM9Zml0LGRjPXZ1dGJyLGRjPWN6
uid: xvrana32
mail: xvrana32

# search result
search: 2
result: 0 Success

# numResponses: 3
# numEntries: 2
```

9. Write a record where there is a mail attribute, a meaningless ll attribute and a uid attribute

```
$ ldapsearch -H ldap://localhost:389 -x -z 0 -b "dc=vutbr,dc=cz" 'uid=xvorob
02' uid ll mail


# extended LDIF
#
# LDAPv3
# base <dc=vutbr,dc=cz> with scope subtree
# filter: uid=xvorob02
# requesting: uid ll mail
#

# xvorob02, fit.vutbr.cz
dn: uid=xvorob02,dc=fit,dc=vutbr,dc=cz
mail: xvorob02

# search result
search: 2
result: 0 Success

# numResponses: 2
# numEntries: 1
```

## 📗 Resources

LDAP - Wikipedia. (2023)
[https://cs.wikipedia.org/wiki/LDAP]
DigitalOcean. (2023). Understanding the LDAP Protocol, Data Hierarchy, and Entry Components. [https://www.digitalocean.com/community/tutorials/understanding-the-ldap-protocol-data-hierarchy-and-entry-components]
OSS Nokalva. (2023). Basic encoding rules. [https://www.oss.com/asn1/resources/asn1-made-simple/asn1-quick-reference/basic-encoding-rules.html]
Data tracker. RFC steam IETF. (1995). Tim Howes, Steve Kille, Wengyik Yeong. [https://datatracker.ietf.org/doc/html/rfc1777]
NESFIT/IPK-Projects. (2023).
[https://git.fit.vutbr.cz/NESFIT/IPK-Projekty/src/branch/master]

## ⚖️ License

See [LICENSE](LICENSE).
