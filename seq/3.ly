\version "2.22.1"
\header {
  title = "Simple Example"
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
      \key c \major
      \time 3/2
      \set Score.currentBarNumber = #1
      \override Score.BarNumber.break-visibility = ##(#f #f #t)
      \override Score.BarNumber.self-alignment-X = #CENTER
      \set Score.barNumberVisibility = #all-bar-numbers-visible
      c'2 d'2 e'2
    }
    \new Staff = "continuo" {
      \clef bass
      \key c \major
      \time 3/2
      <<
        \new Voice = "bassline" {
          c2 d2 e2
        }
        \new FiguredBass {
          \figuremode {
            <_>2 <_\\>2 <_>2
          }
        }
      >>
    }
  >>
}