(defvar x 1)

(let ((y 1)) (print (+ x y) "\n"))

(let ((x 100) (y 2)) (print (+ x y)) (newline))

(let ((a 3))
  (let ((b 4))
    (print "a^2 + b^2 = " (^ a 2) " + " (^ b 2) " = " (+ (^ a 2) (^ b 2)))
  )
)

;; (let ((x)) (print x "\n"))

(let () (newline))

(let ((x "Hello")) (print x ", World!\n")(newline))


