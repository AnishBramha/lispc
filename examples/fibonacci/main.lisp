(defun fib (n)
    
    (if (<= n 1) 1
            (+ (fib (- n 1)) (fib (- n 2)))
    )
)

(print (fib 0) ", ")
(print (fib 1) ", ")
(print (fib 2) ", ")
(print (fib 3) ", ")
(print (fib 4) "\n")





