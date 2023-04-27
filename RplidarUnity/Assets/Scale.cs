using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Scale : MonoBehaviour
{
    public float treshold = .2f;


    // Update is called once per frame
    void Update()
    {
        if (Input.GetKeyDown(KeyCode.DownArrow))
            transform.localScale -= Vector3.one * treshold;
        if (Input.GetKeyDown(KeyCode.UpArrow))
            transform.localScale += Vector3.one * treshold;
        if (Input.GetKeyDown(KeyCode.KeypadPlus))
            ScaleChile(true);
        if (Input.GetKeyDown(KeyCode.KeypadMinus))
            ScaleChile(false);
    }

    private void ScaleChile(bool up)
    {
        for (int i = 1; i < transform.childCount; i++)
        {
            var ch = transform.GetChild(i);
            ch.localScale += Vector3.one * (up ? treshold : -treshold);
        }
    }
}
