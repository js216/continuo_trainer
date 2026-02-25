\version "2.24.2"
\header {
  title = "Purcell bars 48-52"
}

%% Solidus glyph for passing notes in figured bass.
%% A simple diagonal stroke, sized and sloped to match the figure column.
passingNoteSolidus = \markup
  \override #'(thickness . 1.4)
  \draw-line #'(0.55 . 1.4)

#(define-public (continuo-format-bass-figure figure event context)
   (let ((slash (eq? #t (ly:event-property event 'augmented-slash))))
     (if (and slash (not (number? figure)))
         passingNoteSolidus
         (format-bass-figure figure event context))))

\layout {
  \context {
    \Score
    figuredBassFormatter = #continuo-format-bass-figure
  }
}

\score {
  <<
    \new Staff = "melody" {
      \clef treble
      \key g \major
      \time 3/2
      g'2. b'4 b'4 cis''4   d''1.   dis''2. cis''4 dis''2   e''4. fis''8 e''4. d''8 cis''4. b'8   ais'1 fis''2   e''4 d''4 cis''2. b'4   b'1 cis''4. d''8
    }
    \new Staff = "continuo" {
      \clef bass
      \key g \major
      \time 3/2
      <<
        \new Voice = "bassline" {
          g2 fis2 e2 b2 a2 g2 a2 g2 fis2 g2 fis2 e2 fis2 e2 d2 e2 fis2 fis2 g2 fis2 e2
        }
        \new FiguredBass {
          \figuremode {
            <_>2 <_\\>2 <6+>2 <_>2 <_\\>2 <_>2 <2 4+ 6>2 <_\\>2 <6+>2 <6>2 <_\\>2 <6+>2 <_+>2 <_\\>2 <6>2 <_>2 <_\\>2 <_+>2 <_>2 <_\\>2 <6+>2
          }
        }
      >>
    }
  >>
}