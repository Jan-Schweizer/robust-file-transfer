---
title: "A Robust File Transfer Protocol"
abbrev: "arft"
docname: draft-1162-arft-latest
category: info

ipr: trust200902
area: General
workgroup: 13+1149
keyword: Internet-Draft

stand_alone: yes
smart_quotes: no
pi: [toc, sortrefs, symrefs]

author:
 -
    ins: J. Schweizer
    name: Jan Schweizer
    organization: Technische Universität München
    email: j.schweizer@tum.de
 -
    ins: S. Entholzer
    name: Simon Entholzer
    organization: Technische Universität München
    email: simon.entholzer@tum.de
 -
    ins: Y. Jiang
    name: Yi Jiang
    organization: Technische Universität München
    email: yi9.jiang@tum.de
 -
    ins: N. Zbil
    name: Nityananda Zbil
    organization: Technische Universität München
    email: nityananda.zbil@tum.de

normative:

informative:


--- abstract

This is A Robust File Transfer protocol, or ARFT, which is defined to enable a way to transfer files.
The transfer must be reliable and independently of size, even in a lossy or unstable communication environment.
This protocol assumes that the User Datagram Protocol (UDP) is used as the underlying protocol.
This document describes the protocol, its types of messages, and explains the reasoning behind the design decisions.

--- middle

# Introduction

## Overview of ARFT

This document specifies A Robust File Transfer Protocol, or ARFT.
The objective of ARFT is to enable reliable transmission of files between two endpoints in a network, namely server and client.

In ARFT, the server means an instance of the implementation running in server-mode, i.e., the program is continuously listening for new incoming requests and then handling them.
The client means an instance of the implementation running in client-mode, i.e., a user initiates a new file transfer request from the command line.

ARFT is built around a "Pull Approach" design.
A client "pulls" data from the server by asking for chunks with a certain offset into the file.
The server reacts to "pull" messages, but does not send data to the client unless asked for.

To ensure reliability, mechanisms for retransmission, flow control, congestion control, and file validation are implemented.
ARFT uses a bitfield to mark lost packets that have to be resent from the server to a client.
For flow control, the client sets a maximum limit of window size by specifying the maximum throughput at the beginning of transmission.
ARFT adapts Elastic TCP as the congestion control algorithm.
In order to detect wrongly transmitted files or file changes between connection drop and resumption, ARFT uses the SHA256 checksum to support file validation.

In addition, ARFT employs a Client Validation security feature, which gives some protection against DoS attacks.

## Relation to other Protocols

ARFT is implemented directly on top of the stateless User Datagram Protocol (UDP) {{!RFC0768}}, which itself is on top of the Internet Protocol (IP) {{!RFC0791}}.
Unlike TCP {{!RFC0793}} ARFT mainly does not keep a state.
A protocol with a similar goal and functionality would be the Trivial File Transport Protocol (TFTP) {{!RFC1350}}.
The main difference being that for TFTP each packet is acknowledged separately, unlike in ARFT, where data is acknowledged in bulk.
The congestion control used by ARFT is strongly inspired by the TCP Elastic congestion control algorithm.

To increase availability of the server, users are validated before they are allowed to download data.
For this a challenge response authentication takes place.
Most protocols using such a mechanism do so by proofing possession of some shared secret to the server, e.g., a password.
The Salted Challenge Response Authentication Mechanism (SCRAM) {{!RFC5082}} would be an example for such a protocol.
ARFT does not do this to proof authenticity of a user, but rather to have a proof of work.

         +------+
         | ARFT |          Application Level
         +------+
            |
         +-----+
         | UDP |           Transport Layer
         +-----+
            |
    +----------------+
    |    IP & ICMP   |     Network Layer
    +----------------+

# Conventions and Definitions

## Terminology

DoS attack: A denial-of-service attack (DoS) is a cyber-attack in which the perpetrator seeks to make a machine or network resource unavailable to its intended users by temporarily or indefinitely disrupting services of a host connected to a network.

Replay attack: A replay attack is a form of network attack in which valid data transmission is maliciously or fraudulently repeated or delayed.

Man-in-the-middle attack: A man-in-the-middle attack is a cyberattack where the attacker secretly relays and possibly alters the communications between two parties who believe that they are directly communicating with each other, as the attacker has inserted themselves between the two parties.

RTT: The round-trip time is the duration in milliseconds (ms) it takes for a network request to go from a starting point to a destination and back again to the starting point.

CC: Congestion Control controls the entry of data packets into the network, enabling a better use of a shared network infrastructure and avoiding congestive collapse.


## Requirements Language

{::boilerplate bcp14-tagged}

## Notational Conventions

Packet and frame diagrams in this document use a custom format, which is largely inspired by {{!RFC9000}}.
The purpose of this format is to summarize protocol elements.
Prose defines the complete semantics and details of structures.

Complex fields are named and then followed by a list of fields surrounded by a pair of matching braces.
Each field in this list is separated by commas.

Individual fields include length information, plus indications about fixed value, optionality, or repetitions.
Individual fields use the following notational conventions, with all lengths in bits:

- x (A):  Indicates that x is A bits long
- x (...):  Indicates that x can be any length
- x (L) = C:  Indicates that x has a fixed value of C

This document uses network byte order (that is, big endian) values.
Fields are placed starting from the high-order bits of each byte.

Figure 1 provides an example:

    Example Structure {
        One-bit Field (1),
        7-bit Field with Fixed Value (7) = 61,
        Arbitrary-Length Field (...),
    }

# Message Types

This section defines the message types that are used in ARFT.

TABLE 1 lists the message types and names in the connection establishment stage.

    +--------------+--------------------------------------------+
    | Message Type | Name                                       |
    +--------------+--------------------------------------------+
    | 0x00         | Client Initial Request                     |
    | 0x01         | Server Validation Request                  |
    | 0x02         | Client Validation Response                 |
    | 0x03         | Server Initial Response                    |
    +--------------+--------------------------------------------+

TABLE 2 lists the message types and names in the data transmission stage.

	+--------------+--------------------------------------------+
	| Message Type | Name                                       |
	+--------------+--------------------------------------------+
	| 0x04         | Client Transmission Request                |
	| 0x05         | Server Data Response                       |
	| 0x06         | Client Retransmission Request              |
	| 0x07         | Client Finish Message                      |
	+--------------+--------------------------------------------+

TABLE 3 lists the error types and names in ARFT.

	+--------------+--------------------------------------------+
	| Error Type   | Name                                       |
	+--------------+--------------------------------------------+
	| 0x10         | Validation Failed                          |
	| 0x11         | File Not Found                             |
	| 0x12         | Connection Termination                     |
	| 0x13         | Connection Not Found                       |
	+--------------+--------------------------------------------+

## Client Initial Request

Below is the layout of a Client Initial Request packet.

	Client Initial Request {
		type (8) = 0x00,
		filename (...),
	}

The client states the null-terminated name of the requested file stored on the server.

The client sends this Client Initial Request to the server when it wants to download a file from the server.

## Server Validation Request

Below is the layout of a Server Validation Request packet.

	Server Validation Request {
		type (8) = 0x01,
		maskedBits (8),
		maskedHash1 (256),
		hash2 (256),
		nonce (32),
		filename (...),
	}

- maskedBits:
The server specifies the number of bits in hash1 that the client has to brute-force.
The value of maskedBits refers to the least significant bits of hash1.

- maskedHash1:
The masked hash1 generated by the server: the n least significant bits are masked (zeroed out), while the remaining part is the rest of the original hash.
This SHA256 hash is created by using, the file name, the nonce and the server-side secret.

- hash2:
Hash2 is generated by the server, and it is the output of applying a hash function on the full hash1 as input.

- nonce:
The server provides the nonce that it used alongside the server side's secret, and the filename, as input to generate the hash1.

- filename:
The server sends the file name back to the client, so it understands to which file request the Server Validation Request belongs to.

The server sends the Server Validation Request as response to a Client Initial Request.
The goal of the Server Validation Request is to request the client to do a proof-of-work to validate itself.
See [Client Validation](#client-validation) Client Validation for more details.

## Client Validation Response

The client sends the Client Validation Response to the server as response to the Server Validation Request.
Below is the layout of a Client Validation Response packet.

	Client Validation Response {
		type (8) = 0x02,
		hash1 (256),
		nonce (32),
		maxThroughput (16),
		filename (...),
	}

- hash1:
The client gives the solution to the puzzle, i.e., the complete hash1 that hashes to hash2.

- nonce:
The client echos the nonce from the Server Validation Request.

- maxThroughput:
The client specifies the maximum throughput that it can handle during transmission in this field.
This value gives the maximum throughput the client can accept in MB/s.
The range of the maximum throughput it therefore can define lays between 1 MB/s and around 65 GB/s.

- filename:
This field contains the requested file name.
From this, the nonce and the server-side secret, the server calculates a hash value again and compares it to the here given hash1.

The client sends the Client Validation Response to the server as response to the Server Validation Request.

## Server Initial Response

Then the server sends the Server Initial Response to the client if the client passed the client validation, and the file exists.
After the client receives the Server Initial Response from the server, the connection establishment stage ends successfully, and the data transmission stage starts.
If the client does not pass the client validation, the Server Initial Response is not sent.
Instead, the server sends the Validation Failed error message (c.f. [Client Validation Failed](#client-validation-failed)).

Below is the layout of the Server Initial Response packet.

	Server Initial Response {
		type (8) = 0x03,
		connectionID (16),
		fileSize (64),
		checksum (256),
		filename (...),
	}

- connectionID:
It uniquely identifies a connection, i.e., a file request, between a client and a server.
It is assigned by the server after the client validation has completed successfully.
The connectionID that is exchanged with this message is used in all subsequent messages.

- fileSize:
The size of the requested file in bytes.

- checksum:
The server computes the hash value of the requested file using SHA256 and provides it to the client here.

- filename:
Echos the value from the [Client Initial Request](#client-initial-request), so the client can associate the connectionID to the file name.
It is possible for the client to request multiple files at the same time.
The goal of this field is to help the client identify to which file this Server Initial Response refers to.

## Client Transmission Request

After the validation process has completed, and the Server Initial Response arrived, the client sends this packet to request data of the file it wants to download.

	Client Transmission Request {
		type (8) = 0x04,
		connectionID (16),
		windowID (8),
		rtt (32),
		chunkIndex (32),
	}

- windowID:
The client MUST specify the ID of the next window in this field.
The server then will use this ID in the Server Data Responses belonging to that window.

- rtt:
The current Round-Trip Time measured by the client.
The rtt is computed as follows:
The first RTT the client computes by measuring the time between the Client Initial Request, and the Server Validation Request.
For later Client Transmission Requests, the client computes the RTT as the time between the last Client Transmission Request and the first corresponding Server Data Response packet.

- chunkIndex:
The absolute index in the file, where the data for this window begins.

The Client Transmission Request is sent by the client after all the Server Data Response were received correctly [Server Data Response](#server-data-response).
The Client Transmission Request has two roles: it works as an implicit ACK for the last window, and it starts a new window by specifying the starting chunk index.

If the client does not receive a Server Data Response after some timeout, it must resend the Client Transmission Request as it assumes the previous one to be lost.

## Server Data Response

Below is the layout of a Server Data Response packet.

	Server Data Response {
		type (8) = 0x05,
		connectionID (16),
		windowID (8),
		windowSize (16),
		relativeSequenceNumber (16),
		payload (4096),
	}

- windowSize:
The server specifies the number of packets to be sent in this window in this field.
The windowSize MUST be the same for every Server Data Response within a window.
With this field the client understands how large the window is, and how many data chunks belong to the widow.
When it has received all the data of the window, it will calculate the next Client Transmission Request's chunkIndex with this value and the current window's chunkIndex.

- relativeSequenceNumber:
The range of the relativeSequenceNumber is from zero to windowSize minus one.
It is the index of the data chunk in relation to the window's starting absolute chunk index.
The client is able to derive the ordering of data chunks within a window with this sequence number.

- payload:
512 bytes of opaque data.
This field carries the corresponding file chunk.

Server Data Responses are sent by the server when it received either a Client Transmission Request, or a Client Retransmission Request.
If the server receives a Client Transmission Request, it sequentially sends all Server Data Responses in this window.
If the server receives a Client Retransmission Request, it resends the corresponding Server Data Response packets that were marked with 0 in the bitField in Client Retransmission Request.

With the windowID and the relativeSequenceNumber, one chunk from a specific window can be uniquely identified.
These two fields in combination serve the purpose of ensuring correct order for out-of-order arrival of packets.

The client MUST keep record of windowIDs, and the corresponding window size, to be able to calculate the absolute position of data chunks.
Additionally, the client is able to detect duplicate packets that were delayed and ultimately arrive too late at the client, e.g., due to congestion.

## Client Retransmission Request

This packet is sent by the client when lost data packets are detected, but only if at least one Server Data Response was received.
Otherwise, the client does not know the window's size and can therefore not generate the bit field for the lost packets.
Below is the layout of a Client Transmission Request packet.

	Client Retransmission Request {
		type (8) = 0x06,
		connectionID (16),
		windowID (8),
		bitField (...),
	}

- bitField:
The length in bits MUST be equal to the value of the windowSize of the corresponding window.
The bit field indicates missing Server Data Responses from the current window.
The pth bit of the bit field represents the reception of the pth Server Data Response according to the relative sequence number in this window.
The pth bit of the bit field will be marked as 0 either when the client did not receive the pth Server Data Response.
Otherwise, the pth bit of bitField is set to 1.

The client sends the Client Retransmission Request if at least one Server Data Response goes missing in the current window.

## Client Finish Message

Below is the layout of a Client Initial Request packet.

	Client Finish Message {
		type (8) = 0x07,
		connectionID (16),
	}

The client sends the Client Finish Message after having received all file chunks.
Upon receiving the Client Finish Message, the server MUST discard state for this connection.

## Error Messages

To communicate different errors ARFT uses the following error messages.

### Client Validation Failed

Below is the layout of a Client Validation Failed error packet.

	Client Validation Failed {
		type (8) = 0x10,
		filename (...),
	}

The server sends the Client Validation Failed error message to the client if the client fails to pass the client validation.
After receiving the Client Validation Failed, the user decides whether to request the file again or not.

### Server File Not Found

Below is the layout of a Server File Not Found error packet.

	Server File Not Found {
		type (8) = 0x11,
		filename (...),
	}

The server sends the Server File not Found error message to the client if the client passes the client validation, and the file with the given name could not be found.

### Client Connection Termination

Below is the layout of a Client Connection Termination error packet.

	Client Connection Termination {
		type (8) = 0x12,
		connectionID (16),
	}

The client MAY send Client Connection Termination to the server during the data transmission stage.
A connection termination can be caused by different client-side reasons, e.g., interruption caused by the user or no disk space left.

### Server Connection Not Found

Below is the layout of a Server Connection Not Found error packet.

	Server Connection Not Found {
		type (8) = 0x13,
		connectionID (16),
	}

The server sends a Server Connection Not Found error message to the client if the client uses an invalid connectionID during data transmission.
If the client wants to resume connection, it MUST send a new Client Initial Request to the server.
Then the client has to handle a Client Validation again by solving a new puzzle.
After the client passes Client Validation, the connection resumes.
See [Connection Drop](#connection-drop) for more details.

# Robust Communication

## Flow Control

After the initial client validation handshake, when the client sends the client validation response, it provides a maximum throughput (MT) value to the server.
This value is the maximum amount of bytes the client wants to handle per second.
It should be sufficiently large, as it sets an upper limit for the transmission speed.

The throughput of ARFT is RTT dependent.
Only one window gets sent every RTT.
Throughput ist still flexible by increasing or decreasing the window size when the RTT changes.
The server must react to those changes by also increasing or decreasing its maximum window size for a particular connection.
This information is used alongside the congestion control mechanism to decide on the size of the window.

As an example, let us assume the client sets the MT to 1 MB, and the RTT is 1s.
Since a data chunk is defined to have 512 bytes of data, and with the header having 8 more bytes, the Server Data Response contains a total of 520 bytes.
This means that the maximum window size the server must use in this case, can only contain around 2000 Server Data Responses.
If then, e.g., due to better paths becoming available in the network, the RTT shrinks down to 0.5s, the server must adjust its maximum window size for that connection, by halving it as well.
This means the proportion between the server calculated maximum window size, and the RTT must always stay the same.
This ratio reflects the maximum throughput value the client did provide at the beginning of the exchange.

An additional measure a client MAY take if it realizes that the initial set maximum throughput is too large or too small, is to send a connection termination message.
Then the client can restart the transfer with an adjusted maximum throughput value.
It SHOULD keep the previously downloaded data, to continue from where it has left.

## Congestion Control

An important part of ARFT is its congestion control (CC) on the server-side.
Congestion control tries to achieve an adequate throughput of data, while also trying to not overwhelm the network.
As described in the previous [section](#flow-control), for each RTT only one window is exchanged.
As the throughput is proportional to the window size, similar to flow control is congestion control is achieved by increasing or decreasing the window size.
The server should try to approach the client's maximum throughput value, but also react to loss if it occurs.

How to exactly achieve this is described in the following CC algorithm that ARFT uses.
It lends some of its features from TCP Elastic.

In the following we refer to a single connection, of which the server maintains the state.
The first thing the server must keep track of is the current RTT.
That it receives with each Transmission Request from the client.
The server also keeps record of the maximum RTT.
To make CC work the server must also store the current window size (CWS).
It should be set to half of the initial maximum window size, but may be set arbitrarily.
Too small values can have a negative impact especially on the duration of smaller files' transfers.

From the current RTT, maximum RTT and the CWS the server calculates the value of the window weighing function (WWF).
The server does so by taking the ratio between the maximum RTT and the current RTT, then multiplying it with the CWS and lastly taking the square root of the whole expression.

The actual CC consists of two phases:
First the server sends the amount of data that fits in the current window size, and then secondly waits for either a Transmission Request, or a Retransmission Request message and reacts to it.

If the server receives a new Transmission Request, the previous window has been transferred successfully and completely.
In this case the CWS is increased by the outcome of the WWF divided by the CWS.
It must not get increased if the updated window size would be larger than the maximum window size.
Then further data is sent with this updated window size.

In the second case the server receives a Retransmission Request, which means some data packets got lost.
In this retransmission phase the current window size is not adjusted.
From the Retransmission Request the server knows which data packets got lost from in the last window, and only sends those again.
If a subsequent Retransmission Request arrives, this whole process is repeated until the server finally receives a Transmission Request.
Then the CWS from before the retransmission phase is then reduced by some factor defined by the server operator (e.g. halving the CWS).
After this the server proceeds to send more data requested in the last Transmission Request, and the whole process repeats.

The sever MAY allow the server operator to specify a maximum throughput which is shared among all client transfers.
In the case of multiple and parallel file transfers, this further reduces the risk of congestion.

## Packet loss handling

For ARFT there are two kinds of packet losses.
The first type occurs during Client Validation, the second one during the actual data transfer.

Packet loss during the data transfer is handled in the following way: After the client sends a Transmission Request, it receives the Server Data Responses from the server.
The client must keep track of which data chunks it has received, and which are still outstanding.
After a timeout of 1 RTT after the last Server Data Response was received and not everything from the current window has been received, the client creates a Retransmission Request, and sends it to the server.
The server then reacts by updating its congestion control information for that connection.

Packet loss can also occur during the communication to all other sent messages.
Since all messages except the Server Data Responses are sent and handled individually, and not in bulk, the receiver does not notice if they get lost.
If the sender gets no corresponding answer after a timeout of 1 RTT, it simply resends the message.

## Timeouts

In ARFT, each endpoint (client and server) maintains one timeout.

The client-side timeout serves the purpose of setting a time limit in which the server should reply to a message from the client, e.g., Client Initial Request, (Re-)Transmission Request, etc.
If the server does not reply to a message within this timeout, the client assumes that the message was lost and resends the last sent message.
The timeout also serves the purpose of setting a time limit on packet reception within a window.
When the client is receiving packets from a window (has received at least one, but not all from the current window), but no further packets are being received, the client assumes that packets are lost.
The client creates a Retransmission Request from the current window that indicates, which packets are missing and sends the message to the server.
The client-side timeout is reset, whenever the client sends a message to the server or receives a message from the server.
In order to allow for packets being repeatedly dropped in a congested network, the client maintains a retry counter of re-send messages.
For that, the client counts the number of times it tried to reach the server.
If the client does not receive a message from the server after n timeouts, the client assumes that the server has disconnected and clears the so far received file contents and reports an error to the user.
The retry counter is reset to 0 whenever the client receives a message from the server.

The server-side timeout serves the purpose of cleaning up state from clients that are assumed to have disconnected.
For each connection, the server maintains a reasonably long timeout, e.g., 3 minutes.
If the server does not receive a message from a client for that connection within this timeframe, the server assumes that the connection has been lost and cleans up the state for the connection, i.e., discards the state und frees up space.
The server-side timeout is reset, whenever the server sends a message to a client for a connection.
A client can resume the connection any time (c.f. [Connection Drop](#connection-drop)) even if the server has previously discarded state for that connection.
In order to allow for an early clean-up of the state, the client sends the Client Finish Message (c.f. [Client Finish Message](#client-finish-message)) to the server, once the client has received the complete file.

## Connection Drop

Let us assume that a connection might be dropped due to a network failure.
Until a new path in the network is found, the state on the server-side corresponding to the connection ID will be discarded once the server-side timeout for this connection runs out (c.f. [Timeouts](#timeouts)).
Therefore, the client-side MUST consider two cases.
In both cases, the client tries to resume with a Client Transmission Request or a Client Retransmission Request, wherever it left off.

In the first case, the server did not clean up the state corresponding to the connection ID.
The server can immediately resume with Server Data Responses.

In the second case, the server did clean up the state corresponding to the connection ID.
The server MUST respond with a Connection Not Found message.
The client is now aware that it needs to initiate a connection resumption.
In order to resume the connection, the client has to go through the process of Client Validation (c.f. [Client Validation](#client-validation)) again.
After successful validation, the client receives a SHA256 checksum in the Server Initial Response.
The client can compare this checksum with the SHA256 checksum that it has stored from the previous connection before the resumption.
If the checksums are still the same, the client knows that the file has not changed between connection drop and connection resumption.
The client sends a Transmission Request with the new connection ID that was assigned to it by the server in the last Server Initial Response and the chunk index into the file at which it wants to resume the file transfer.

If the checksum are not the same, the client knows that the file has changed in the meantime.
It discards all previous file contents and sends a Transmission Request with the new connection id and a chunk index of 0 to the server.
This essentially starts over the file transfer after the file has changed.

## Connection Migration

Each time the server receives a Transmission Request or Retransmission Request from a client, it updates the endpoint information (IP address) in the associated state for the connection ID that is sent along the request.
While this approach makes connection migration very easy and straightforward, it also opens up the protocol for various security issues which are discussed in [Security Considerations](#security-considerations) (assignment 3).

# Security Considerations

## File Validation

The server computes the SHA256 checksum of the file being requested after a client has been successfully validated and sends the checksum to the client in the Server Initial Response.
The client stores this checksum.
After the client has received the file completely, it validates the contents of the file by computing the SHA256 checksum over the file itself.
In case the checksums, the one received from the server, and the one computed by the client, do not match, the client SHOULD discard the file content and notify the user about the error.
The client does NOT issue a retransmission of the file to the server, but leaves this decision to the user.

## Client Validation

Before a server accepts an incoming file request from a client, it asks the client to solve a puzzle in order to validate itself.
The purpose of this puzzle is to deter possible attackers from trying a DoS attack on the server, by asking for a proof-of-work for each file request and distinguish a valid client from a malicious attacker.
The computational overhead is neglectable for valid clients that only have to compute a single puzzle for their request.
However, the overhead is rather heavy for an attacker that fires multiple file requests at once.
Additionally, the server SHOULD NOT store any kind of state before a client has validated itself.

The Client Validation mechanism works as follows:
Upon receiving a Client Initial Request, the server generates a random number r.
It then computes a hash1 from this r, the requested filename and a server-side secret.
This hash1 serves again as input for a second hash function in order to compute a hash2.
The hash functions that are used to compute hash1 and hash2 can be the same or different but must be suitable hash-functions (e.g. SHA256).
The hash function to compute hash2 MUST be made publicly known as every client needs to use that function.
The server masks (zeroes out) some number of bits from the hash1.
As of convention, the least significant n bits of hash1 are masked.
It sends a Server Validation Request that carries the r, the masked hash1, the number of masked bits, the full hash2, and the filename back to the client.
The client is tasked to brute-force hash1 with its masked bits.
It has to try each possibility for the masked bits, hash the candidate and see if the hash matches hash2.
The hardness of the puzzle can be controlled with the number of masked bits.
If k bits are masked, then this requires up to 2^k trials.
This is easily done for small k, but requires some computational effort for larger k.
The client cannot forge fake puzzles as long as it does not know the server's secret.
Once the client has found the solution to the puzzle, i.e., the hash1 that hashes to hash2, it sends a Client Validation Response to the server.
This message contains the r, the full hash1 and the filename.
The server is now able to easily recompute hash1 from the r, the filename and its server secret and compares the computed hash1 with the hash1 that is provided by the client.
If the hashes match, the server initiates state for that connection.
It assigns a connection ID, collects some metadata about the requested file and sends a Server Initial Response with that data to the client.
The connection ID allows the client to start pulling file data from the server with Transmission Requests.
If the hashes do not match, the server reports an error message back to the client, telling that the request is denied.
The client reports an error to the user, who can then issue another request.

The number of masked bits in hash1, and thus the hardness of the puzzle, can be controlled by the server.
The server MAY start in either case with a low number of masked bits in order to allow clients with limited CPU power to initiate a connection, e.g., a Raspberry PI.
The server MAY decide to dynamically increase the hardness of the puzzle if it suspects malicious activities, e.g., multiple file requests coming rapidly from the same IP address.
In this case, the server gradually increases the number of masked bits for clients that exhibit such conspicuous behavior.

The current design is susceptible to replay attacks.
Once an attacker has solved a single puzzle, they can easily reuse the same (valid) data for multiple Client Validation Responses.
As one possible approach to prevent this type of attack, a server MAY use a timestamp as the random number r.
The timestamp is only valid for a limited amount of time and expires thereafter.
If the expiration time is chosen roughly to match the time it takes to find a solution to the puzzle, an attacker cannot reuse the valid data for multiple requests that easily.

## Man-in-the-middle attacks

The current design of the ARFT protocol is susceptible to various Man-in-the-middle attacks.
Since neither the client nor the server encrypt or otherwise secure the communication, an attacker can easily intercept and alter messages between endpoints.
An attacker might change arbitrary fields during transmission messages.
For example, an attacker could change the windowSize field in a Server Data Response message.
The client would adapt the wrong window size and send Transmission Requests for the next chunks, although they already have been sent out by the server.
Second, an attacker is able to alter the file checksum in the Server Initial Response.
As a result, even if the original file received by the client is correct, the file is discarded because its checksum does not match the (altered) file checksum from the Server Initial Response.
Such kind of attacks would slow down the file transfer by increasing the traffic overhead, or even subvert the final transmission result.

The injection of Client Finish Messages by an attacker, although the file is not transferred completely, is another kind of attack that would considerably slow down the file transfer.
As a consequence, the server would discard the state for this file transfer and a subsequent Transmission Request would require the client to again go through the client validation process.
Moreover, an attacker could maliciously drop the transmission process by injecting a Client Connection Termination error message to the server, which also would require the client do validate itself again.
Both of these attacks would not only slow down the file transfer, but also increase power consumption on the client side if done repeatedly.
An attacker is also able to completely deny a file transfer, if they send a File Not Found error message to the client as response to Client Validation Response.

A more severe attack would be to hijack the whole file transmission.
After the client has done the proof-of-work for the client validation, an attacker might intercept all packets from the client to the server.
The client would eventually stop trying to reach the server, thinking it has disappeared.
Since client migration is not protected at all and being handled by the server by just updating the endpoint information on each message, an attacker can easily take control over the connection and pretend to be the client.
The server would send the file to an unknown (and unvalidated) attacker.
The other way around, an attacker can capture a connection and pose as a server.
The attacker would be able to send arbitrary files to the client that might contain malicious code with harmfull effects when executed.

## Security measures

In order to guarantee a secure transmission, ARFT needs to sign and/or encrypt each message on both sides.
Even if ARFT would only protect parts of a message (encrypt the files but not the rest of the commands), an attacker might be able to do considerable damage, by slowing down the file transfer with fake Client Finish Messages or hijack a file transfer by stealing the connection id.
An established way around these issues is to use public-key cryptography, where each message is signed by an endpoint with their respective private key that can be validated by the other side with with the matching public key.
This of course poses the issue of how to exchange the public keys.
ARFT's connection establishment process could be extended to also support a secure key exchange, which might be hard to do correct.
Another possibility is to assume that the keys have been exchanged prior to a file transfer over a different medium and are ready to be used within the protocol.



# Future Extensions

## File Validation

A checksum is used to verify that the file, which is transferred from a server to a client, is transmitted and assembled correctly.
The checksum is the product of a SHA256 hash of the whole file.

Once the file transfer has been completed, and the client finds that the computed file checksum is different from the checksum that it has received in the Server Initial Response, it cannot trace back which chunk(s) are incorrect.

Note: The protocol SHOULD support files as large as 10 GB.
Consider that it takes a magnitude of 1 minute to compute the checksum of a 10 GB file, e.g., an archive file of a popular IDE, Xcode:

    # Intel i9-8950HK (12) @ 2.90GHz.
    # 10_783_587_696 byte Xcode_13.4.1.xip
    $ time shasum -a 256 ~/Xcode_13.4.1.xip
    a1e0dbd6d5a96c4a6d3d63600b58486759aa836c2d9f7e8fa6d7da4c7399638b  ~/Xcode_13.4.1.xip
    shasum -a 256 ~/Xcode_13.4.1.xip  41.61s user 2.16s system 99% cpu 43.921 total

It is RECOMMENDED that the server caches the checksums for files that were requested at least once.
The cache MUST be invalidated, if the file contents change.
The server MAY pre-compute hashes for files in a directory that it watches.

Nonetheless, the client SHOULD be prepared to keep a long enough timeout for the server to compute the checksum.
On the other hand, the server MAY intelligently adjust the time, that the client needs for the client validation, to the time, that itself needs to compute the checksum.

## Multiple File Transfer

ARFT was designed with sequential transmission of files in mind.
The protocol MAY be used for multiple file transfer.

### Server-side

At the server-side, a source IP address MAY already not be unique.
Additionally, from Connection Migration it holds: If a connection ID is used from another IP address, then the connection is updated with the new IP address.
Vice versa, **it does not hold**: If a source ID address uses a second connection ID, then the IP address is updated with the new connection ID.

Thus, client-side, one source IP address MAY be associated with N connection IDs.

### Client-side

Client-side, the program MAY create multiple coroutines
Each coroutine handles one file.

However, the CC might not be optimal for multiple connections per client.
In order to efficiently support parallel file transfer, an implementation of the current design could analyze the behavior of the congestion control and potentially employ a more nuanced congestion control algorithm.
Second, the client matches requests and responses with the filename (c.f. [Server Validation Request](#server-validation-request) [Server Initial Response](#server-initial-response)).

## Two-way Transmission

Two-way transmission is not defined in this version of the protocol.
The use case might be considered in future versions.

This draft version does not declare a message type that would indicate a reversal of client- and server-side roles.
To accomplish two-way transmission, we may first introduce a new message type:

    Client Initial Request _Reverse_ {
        type (8) = 0xXX,
        filename (...),
    }

The purpose of this message is to get the server to act like a client and send a Client Initial Request to the client.
The server uses `filename` of the Client Initial Request _Reverse_ in its Client Initial Request.
Conversely, the client acts now as a server after it sends the Client Initial Request _Reverse_ and listens to ARFT packets.
The transfer responsibilities are now reversed.

Does Client Validation need to be more complex, if a client is allowed to upload arbitrary data to a server?
This question should be considered in future versions.

--- back
