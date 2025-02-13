/*
   Network Next Buyer Key Generator
   Copyright © 2017 - 2025 Network Next, Inc. All rights reserved.
*/

package main

import (
	"crypto/ed25519"
	"crypto/rand"
	"encoding/base64"
	"fmt"
	"log"
)

func main() {

	buyerId := make([]byte, 8)
	rand.Read(buyerId)

	publicKey, privateKey, err := ed25519.GenerateKey(nil)
	if err != nil {
		log.Fatalln(err)
	}

	buyerPublicKey := make([]byte, 0)
	buyerPublicKey = append(buyerPublicKey, buyerId...)
	buyerPublicKey = append(buyerPublicKey, publicKey...)

	buyerPrivateKey := make([]byte, 0)
	buyerPrivateKey = append(buyerPrivateKey, buyerId...)
	buyerPrivateKey = append(buyerPrivateKey, privateKey...)

	fmt.Printf("\nWelcome to Network Next!\n\n")
	fmt.Printf("This is your public key:\n\n    %s\n\n", base64.StdEncoding.EncodeToString(buyerPublicKey[:]))
	fmt.Printf("This is your private key:\n\n    %s\n\n", base64.StdEncoding.EncodeToString(buyerPrivateKey[:]))
	fmt.Printf("IMPORTANT: Save your private key in a secure place and don't share it with anybody, not even us!\n\n")
}
