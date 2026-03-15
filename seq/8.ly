\version "2.22.1"
\header {
  title = "An Evening Hymn"
  composer = "Henry Purcell"
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
      \set Score.currentBarNumber = #21
      \override Score.BarNumber.break-visibility = ##(#f #f #t)
      \override Score.BarNumber.self-alignment-X = #CENTER
      \set Score.barNumberVisibility = #all-bar-numbers-visible
      b'1 g'2 d''1 r2 g''2. fis''4 e''2 e''4 f''2. e''4 d''4 a'1 d''2
    }
    \new Staff = "continuo" {
      \clef bass
      \key g \major
      \time 3/2
      <<
        \new Voice = "bassline" {
          g2 fis2 e2 fis2 e2 d2 e2 d2 c2 d2 c2 b,2 c2 d2 d2
        }
        \new FiguredBass {
          \figuremode {
            <_>2 <_\\>2 <6>2 <6>2 <_\\>2 <6>2 <_>2 <_\\>2 <_>2 <_+>2 <_\\>2 <6>2 <6>2 <4>2 <3>2
          }
        }
      >>
    }
  >>
}