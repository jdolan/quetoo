;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This script was donated by 'saulgoode' over at the GIMP talk forums.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(define (save-normal-map file-pattern)
  (define (save-layer orig-image layer name)
    (let* (
        (image)
        (buffer)
        )
      (set! buffer (car (gimp-edit-named-copy layer "temp-copy")))
      (set! image (car (gimp-edit-named-paste-as-new buffer)))
      (when (and (not (= (car (gimp-image-base-type image)) INDEXED))
                 (string-ci=? (car (last (strbreakup name "."))) "gif"))
        (gimp-image-convert-indexed image
                                    NO-DITHER
                                    MAKE-PALETTE
                                    255
                                    FALSE
                                    FALSE
                                    "")
        )
      (gimp-file-save RUN-NONINTERACTIVE image (car (gimp-image-get-active-layer image)) name name)
      (gimp-buffer-delete buffer)
      (gimp-image-delete image)
      )
    )
  (let* (
      (filelist (cadr (file-glob file-pattern 1)))
      (filename)
      (ext)
      (image)
      (drawable)
      )
    (while (not (null? filelist))
      (set! ext "png") ;; failsafe if filename has no extension
      (set! filename (car filelist))
      (set! image (car (gimp-file-load RUN-NONINTERACTIVE filename filename)))
      ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
      ;; convert filename.txt to filename_nm.ext
      (set! filename (strbreakup filename "."))
      (when (> (length filename) 1)
        (set! ext (car (last filename)))
        (set! filename (butlast filename))
        )
      (set! filename (unbreakupstr filename "."))
      (set! filename (string-append filename "_nm." ext))
      ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
      ;; assure that image is a single-layer RGBA
      (when (= (car (gimp-image-base-type image)) INDEXED)
        (gimp-image-convert-rgb image)
        )
      (set! drawable (car (gimp-image-merge-visible-layers image CLIP-TO-IMAGE)))
	  (gimp-layer-add-alpha drawable)
      ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
      ;; Run the filter
      (plug-in-normalmap RUN-NONINTERACTIVE image drawable
              8     ; filter (0 = 4 sample, 1 = sobel 3x3, 2 = sobel 5x5,
                    ;         3 = prewitt 3x3, 4 = prewitt 5x5,
                    ;         5-8 = 3x3,5x5,7x7,9x9)
              0.0   ; minz (0 to 1)
              10.0   ; scale (>0)
              FALSE ; wrap
              0     ; height_source (0=RGB, 1=Alpha)
              1     ; Alpha (0 = unchanged, 1 = set to height,
                    ;        2 = set to inverse height, 3 = set to 0,
                    ;        4 = set to 1, 5 = invert,
                    ;        6 = set to alpha map value)
              0     ; conversion (0 = normalize only, 1 = Biased RGB,
                    ;             2 = Red, 3 = Green, 4 = Blue, 5 = Max RGB,
                    ;             6 = Min RGB, 7 = Colorspace,
                    ;             8 = Normalize only, 9 = Convert to height map)
              0     ; DU/DV map (0 = none, 1 = 8-bit, 2 = 8-bit unsigned,
                    ;            3 = 16-bit, 4 = 16-bit unsigned)
              FALSE ; xinvert
              FALSE ; yinvert
              FALSE ; swapRGB
              0.0   ; contrast
              drawable ; alphamapid (using 'drawable' is a bit of a kludge)
              )
      ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
      ;; save the file
      (save-layer image drawable filename)
      (gimp-image-delete image)
      (set! display 0)
      (set! filelist (cdr filelist))
      )
    )
  )
