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
      \set Score.currentBarNumber = #16
      \override Score.BarNumber.break-visibility = ##(#f #f #t)
      \override Score.BarNumber.self-alignment-X = #CENTER
      \set Score.barNumberVisibility = #all-bar-numbers-visible
      b'1 e''4 e''4 e''2 d''2. d''4 d''2 c''1 c''2 b'2. b'4 b'4 a'4 a'2 b'4. c''8
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
            <_>2 <_\\>2 <6>2 <6>2 <_\\>2 <6>2 <_>2 <_\\>2 <_>2 <_>2 <_\\>2 <6>2 <7>2 <6>2 <4>2
          }
        }
      >>
    }
  >>
}