# isa23ldap-server

## 📚 Introduction

Primitive LDAPv2 server ISA project from 2023.

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

After processing the arguments, a tcp connection is set up. There is a tcp function for this which was taken from NESFIT, IPK projects. [5] In the function, a socket is created and bound to, and then it listens. Once it catches a bindRequest packet, it responds with a bindResponse and transitions to the searchRequest waiting state. After it is captured, the get_message function is called.
