Name: Ryan Berlin
Partner: none

Error Codes Handled: 1, 4, 5

I did not use alarm to support the the server timing out as the assignment seemed to suggest.
Insead, I used setsockopt() to set recvfrom to timout after 1 second, and incremented a counter
to see if it timedout 10 times in a row.