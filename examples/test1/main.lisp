(let (x (y "Hello!")) 
    (print "x -> " x "\n")
    (print "y -> " y)
    (newline)
)

(defun foo (x y z)
    
    (let ((acc z))
        
        (print (+ acc (+ x y)))
        (newline)
    )
)


(defun bar ()
    (print "\n")
    (newline)
    ()
    (+ 1 2)
)

(foo 1 2 3)
(bar)




