(print (eql 5 5) "\n")

(print  (not

(and (/= 2 3)

       (or
            (> 2 5) 2 #t
       )

       (<= 3 4)))
"\n")


(print (eql #t #t) " " (eql #t #f) " " (eql #f #f) "\n")



